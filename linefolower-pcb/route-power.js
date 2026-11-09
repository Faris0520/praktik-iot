// Widen the 3.3 V and 5 V tracks, then rebuild the ground pour around them.

stack({
  boardThicknessMm: 1.6,
  fallbackCopperThicknessOz: 1,
  layers: [
    { kind: "copper", name: "TOP", thicknessOz: 1 },
    { kind: "dielectric", name: "FR4", thicknessMm: 1.5, relativePermittivity: 4.3 },
    { kind: "copper", name: "BOTTOM", thicknessOz: 1 },
  ],
});

drc({
  trackWidthMm: 0.25,
  minTrackWidthMm: 0.2,
  clearanceMm: 0.2,
  edgeClearanceMm: 0.3,
  via: { diameterMm: 0.6, drillMm: 0.3, minDiameterMm: 0.6, minDrillMm: 0.3 },
});

netClass("PWR", {
  nets: ["+3V3", "+5V"],
  trackWidthMm: 0.6,
  minTrackWidthMm: 0.25,
  clearanceMm: 0.2,
});

powerNet("+3V3", {
  maxCurrentA: 0.8,
  maxTempRiseC: 10,
  maxTrackWidthMm: 1,
  priority: "high",
});
powerNet("+5V", {
  maxCurrentA: 0.8,
  maxTempRiseC: 10,
  maxTrackWidthMm: 1,
  priority: "high",
});

clearRouting({ nets: ["+3V3", "+5V"], items: ["tracks", "vias"] });
onlyNets("+3V3", "+5V");
ignoreNets("GND");

plane({
  net: "GND",
  layers: "OUTER",
  region: board(),
  stitching: false,
  zone: {
    clearanceMm: 0.2,
    minThicknessMm: 0.25,
    padConnection: { mode: "solid" },
    removeIslandsBelowMm2: 2,
  },
});

runAll();
