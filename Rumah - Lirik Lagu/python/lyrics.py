
import asyncio
import re
import time
import unicodedata
from datetime import datetime, timezone
from email.utils import parsedate_to_datetime

import requests
import serial
from serial.tools import list_ports
from winrt.windows.media.control import (
    GlobalSystemMediaTransportControlsSessionManager
    as MediaManager
)

LRCLIB_URL = "https://lrclib.net"
BAUD_RATE = 115200
POLL_INTERVAL = 0.5
MIN_LRCLIB_GAP = 0.4
MAX_RETRY_AFTER = 60.0
CLIENT_ID = "ESP32-OLED-Lyrics/1.0 (personal IoT lyrics display)"

HEADERS = {
    "User-Agent": CLIENT_ID,
    "Lrclib-Client": CLIENT_ID,
}

http = requests.Session()
http.headers.update(HEADERS)

_last_lrclib_request = 0.0
_lrclib_retry_until = 0.0
_lyrics_cache = {}


class LrclibRateLimited(Exception):
    def __init__(self, retry_after):
        super().__init__(
            f"LRCLIB membatasi permintaan, coba lagi dalam {retry_after:.0f}s"
        )
        self.retry_after = retry_after


def normalize(text):
    """Normalisasi teks untuk mencocokkan judul dan artis."""
    text = unicodedata.normalize("NFKD", text or "")
    return "".join(c for c in text.casefold() if c.isalnum())


def timedelta_seconds(value):
    if value is None:
        return 0.0
    if hasattr(value, "total_seconds"):
        return float(value.total_seconds())
    return float(getattr(value, "seconds", 0) or 0)


def retry_after_seconds(header, fallback=5.0):
    """Parse Retry-After sebagai detik atau HTTP-date."""
    if not header:
        return fallback

    header = header.strip()

    try:
        return max(1.0, float(header))
    except ValueError:
        pass

    try:
        when = parsedate_to_datetime(header)
    except (TypeError, ValueError, IndexError):
        return fallback

    if when.tzinfo is None:
        when = when.replace(tzinfo=timezone.utc)

    return max(1.0, (when - datetime.now(timezone.utc)).total_seconds())


def remaining_lrclib_wait():
    return max(0.0, _lrclib_retry_until - time.monotonic())


def remember_rate_limit(response):
    global _lrclib_retry_until
    wait = retry_after_seconds(response.headers.get("Retry-After"), 5.0)
    wait = min(MAX_RETRY_AFTER, wait)
    _lrclib_retry_until = time.monotonic() + wait
    return wait


def lrclib_get(path, params):
    """GET ke LRCLIB dengan jeda antar-request dan hormat ke Retry-After."""
    global _last_lrclib_request

    wait = remaining_lrclib_wait()
    if wait > 0:
        raise LrclibRateLimited(wait)

    gap = MIN_LRCLIB_GAP - (time.monotonic() - _last_lrclib_request)
    if gap > 0:
        time.sleep(gap)

    response = http.get(
        f"{LRCLIB_URL}{path}",
        params=params,
        timeout=8
    )
    _last_lrclib_request = time.monotonic()

    if response.status_code in (429, 503):
        wait = remember_rate_limit(response)
        raise LrclibRateLimited(wait)

    return response


def parse_synced_lyrics(raw_lyrics):
    """Mengubah format LRC menjadi daftar (waktu, lirik)."""
    timestamp_pattern = re.compile(
        r"\[(\d{1,3}):(\d{2})(?:[.:](\d{1,3}))?\]"
    )

    result = []

    for raw_line in (raw_lyrics or "").splitlines():
        timestamps = list(timestamp_pattern.finditer(raw_line))

        if not timestamps:
            continue

        text = timestamp_pattern.sub("", raw_line).strip()

        # Abaikan baris metadata atau baris kosong.
        if not text or re.match(r"^\[[a-zA-Z]+:", text):
            continue

        for match in timestamps:
            minutes = int(match.group(1))
            seconds = int(match.group(2))
            fraction = match.group(3) or "0"

            fraction_seconds = int(fraction) / (10 ** len(fraction))
            timestamp = minutes * 60 + seconds + fraction_seconds

            result.append((timestamp, text))

    result.sort(key=lambda item: item[0])
    return result


def fetch_synced_lyrics(title, artist, album, duration):
    """Mencari lirik bertimestamp dari LRCLIB."""

    cache_key = (normalize(title), normalize(artist))
    if cache_key in _lyrics_cache:
        return _lyrics_cache[cache_key]

    params = {
        "track_name": title,
        "artist_name": artist,
    }

    if album:
        params["album_name"] = album

    if 1 < duration < 3600:
        params["duration"] = round(duration)

    try:
        response = lrclib_get("/api/get", params)

        if response.status_code == 200:
            data = response.json()
            lyrics = parse_synced_lyrics(
                data.get("syncedLyrics")
            )

            if lyrics:
                _lyrics_cache[cache_key] = lyrics
                return lyrics

    except LrclibRateLimited:
        raise
    except requests.RequestException as error:
        print(f"Permintaan LRCLIB gagal: {error}")

    try:
        response = lrclib_get(
            "/api/search",
            {
                "track_name": title,
                "artist_name": artist,
            }
        )
        response.raise_for_status()

        candidates = response.json()

        # Hanya pilih hasil yang memiliki lirik bertimestamp.
        candidates = [
            item for item in candidates
            if item.get("syncedLyrics")
        ]

        title_key = normalize(title)
        artist_key = normalize(artist)

        def candidate_score(item):
            candidate_title = normalize(item.get("trackName"))
            candidate_artist = normalize(item.get("artistName"))

            title_score = (
                0 if candidate_title == title_key
                else 1 if title_key in candidate_title
                or candidate_title in title_key
                else 2
            )

            artist_score = (
                0 if candidate_artist == artist_key
                else 1 if artist_key in candidate_artist
                or candidate_artist in artist_key
                else 2
            )

            try:
                duration_difference = abs(
                    float(item.get("duration", 0)) - duration
                )
            except (TypeError, ValueError):
                duration_difference = 9999

            return (
                title_score,
                artist_score,
                duration_difference
            )

        if not candidates:
            _lyrics_cache[cache_key] = []
            return []

        best = min(candidates, key=candidate_score)

        # Hindari memilih lagu dengan judul berbeda.
        if candidate_score(best)[0] == 2:
            _lyrics_cache[cache_key] = []
            return []

        lyrics = parse_synced_lyrics(best.get("syncedLyrics"))
        _lyrics_cache[cache_key] = lyrics
        return lyrics

    except LrclibRateLimited:
        raise
    except (requests.RequestException, ValueError) as error:
        print(f"Gagal mencari lirik: {error}")
        return []


async def read_current_media(manager):
    """Membaca sesi media aktif Windows."""

    session = manager.get_current_session()

    if session is None:
        return None

    properties = await session.try_get_media_properties_async()

    if not properties or not properties.title:
        return None

    playback = session.get_playback_info()
    timeline = session.get_timeline_properties()

    title = properties.title or ""
    artist = properties.artist or "Artis tidak diketahui"
    album = properties.album_title or ""

    position = timedelta_seconds(timeline.position)
    duration = timedelta_seconds(timeline.end_time)

    # Perkirakan posisi terkini ketika lagu sedang dimainkan.
    if playback.playback_status.name == "PLAYING":
        try:
            elapsed = time.time() - timeline.last_updated_time.timestamp()
            position += max(0.0, elapsed)
        except (AttributeError, OSError, ValueError):
            pass

    position = max(0.0, position)

    if duration > 0:
        position = min(position, duration)

    return {
        "title": title,
        "artist": artist,
        "album": album,
        "duration": duration,
        "position": position,
        "status": playback.playback_status.name,
    }


def choose_serial_port():
    """Memilih port COM ESP32."""

    ports = list(list_ports.comports())

    if not ports:
        return input("Masukkan port ESP32, misalnya COM5: ").strip()

    print("\nPort serial yang tersedia:")

    for index, port in enumerate(ports):
        print(f"{index}. {port.device} - {port.description}")

    while True:
        try:
            choice = int(input("Pilih nomor port ESP32: "))

            if 0 <= choice < len(ports):
                return ports[choice].device

        except ValueError:
            pass

        print("Pilihan tidak valid. Coba lagi.")


def send_to_oled(esp, text):
    """Mengirim satu baris teks ke ESP32."""

    clean_text = (text or "").replace("\r", " ").replace("\n", " ")
    esp.write((clean_text + "\n").encode("utf-8", errors="replace"))
    esp.flush()


def find_current_line(lyrics, position):
    """Mengambil baris lirik terakhir sesuai posisi lagu."""

    current = None

    for timestamp, text in lyrics:
        if timestamp > position:
            break
        current = text

    return current


async def monitor_spotify(esp):
    manager = await MediaManager.request_async()

    current_track_key = None
    current_lyrics = []
    last_displayed_text = None
    media_was_missing = False

    print("\nMonitor lirik aktif.")
    print("Putar lagu di Spotify untuk memulai.")
    print("Tekan Ctrl+C untuk berhenti.\n")

    while True:
        try:
            media = await read_current_media(manager)

            if media is None:
                if not media_was_missing:
                    message = "PUTAR LAGU DI SPOTIFY"
                    send_to_oled(esp, message)
                    last_displayed_text = message
                    media_was_missing = True

                await asyncio.sleep(POLL_INTERVAL)
                continue

            media_was_missing = False

            title = media["title"]
            artist = media["artist"]
            album = media["album"]
            duration = media["duration"]
            position = media["position"]

            track_key = (
                normalize(title),
                normalize(artist)
            )

            # Cari lirik hanya ketika judul/artis berubah, bukan saat durasi update.
            if track_key != current_track_key:
                wait = remaining_lrclib_wait()
                if wait > 0:
                    message = "TUNGGU LRCLIB..."
                    if message != last_displayed_text:
                        send_to_oled(esp, message)
                        last_displayed_text = message
                        print(
                            f"LRCLIB rate limit, menunggu {wait:.0f}s..."
                        )
                    await asyncio.sleep(POLL_INTERVAL)
                    continue

                print(f"\nLagu: {title} - {artist}")

                message = "MENCARI LIRIK..."
                send_to_oled(esp, message)
                last_displayed_text = message

                try:
                    current_lyrics = fetch_synced_lyrics(
                        title, artist, album, duration
                    )
                except LrclibRateLimited as error:
                    print(error)
                    await asyncio.sleep(POLL_INTERVAL)
                    continue

                current_track_key = track_key

                if current_lyrics:
                    print(
                        f"Lirik ditemukan: {len(current_lyrics)} baris."
                    )
                else:
                    print(
                        "Lirik bertimestamp tidak ditemukan untuk lagu ini."
                    )

            if current_lyrics:
                lyric = find_current_line(current_lyrics, position)

                if lyric is None:
                    lyric = f"{title} - {artist}"

            else:
                lyric = "LIRIK TIDAK TERSEDIA"

            # Kirim hanya ketika isi tampilan berubah.
            if lyric != last_displayed_text:
                send_to_oled(esp, lyric)
                last_displayed_text = lyric
                print(f"[{position:.1f}s] {lyric}")

        except Exception as error:
            print(f"Terjadi kesalahan: {error}")
            await asyncio.sleep(1)

        await asyncio.sleep(POLL_INTERVAL)


def main():
    port = choose_serial_port()

    try:
        with serial.Serial(port, BAUD_RATE, timeout=1) as esp:
            # Membuka port biasanya me-reset ESP32.
            time.sleep(2)
            asyncio.run(monitor_spotify(esp))

    except KeyboardInterrupt:
        print("\nProgram dihentikan.")

    except serial.SerialException as error:
        print(f"Gagal membuka port serial: {error}")


if __name__ == "__main__":
    main()