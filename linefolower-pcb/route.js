// Two-layer line follower. Signals and power on both outer layers.
// GND is a pour on top and bottom so SMD pads connect without ground tracks.

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
  holeToHoleClearanceMm: 0.5,
  via: {
    diameterMm: 0.6,
    drillMm: 0.3,
    minDiameterMm: 0.6,
    minDrillMm: 0.3,
  },
});

powerNet("VBAT", {
  maxCurrentA: 2,
  maxTempRiseC: 10,
  maxTrackWidthMm: 1.5,
  priority: "critical",
  viaPreference: "avoid",
});
powerNet("+5V", {
  maxCurrentA: 0.8,
  maxTempRiseC: 10,
  maxTrackWidthMm: 1,
  priority: "high",
});
powerNet("+3V3", {
  maxCurrentA: 0.8,
  maxTempRiseC: 10,
  maxTrackWidthMm: 1,
  priority: "high",
});

ignoreNets("GND");

plane({
  net: "GND",
  layers: "OUTER",
  region: board(),
  zone: {
    clearanceMm: 0.2,
    minThicknessMm: 0.25,
    padConnection: { mode: "solid" },
    removeIslandsBelowMm2: 2,
  },
  stitching: {
    gridMm: 5,
    via: "drc-min",
  },
});

runAll();
