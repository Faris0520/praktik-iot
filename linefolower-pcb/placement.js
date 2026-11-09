// Line follower controller. New outline, full placement.
// 90 x 60 mm, origin at center. Positive X is right, positive Y is down.

board.rect(90, 60, {
  layers: ["top"],
  defaultLayer: "top",
  clearance: 0.45,
  edge: 1.2,
});

boardHole.corners({
  drill: 3.2,
  diameter: 6.2,
  inset: 4.5,
  keepout: 4,
  prefix: "MH",
});

silkscreen.designators({ height: 1, margin: 0.3 });

block("mcu", ["U1", "R1", "R2"], "mcu");
block("mcu_decoupling", ["C4", "C5"], "mcu", null, {
  placement: "satellite",
  attachTo: "mcu",
  anchor: pin("U1", "2"),
});
block("regulator", ["U2", "C1"], "power");
block("regulator_out", ["C2", "C3"], "power", null, {
  placement: "satellite",
  attachTo: "regulator",
  anchor: pin("U2", "2"),
});
block("battery", ["J1", "J2"], "connector");
block("l298_io", ["J6"], "connector");
block("sensor", ["J5"], "sensor");
block("oled", ["J4"], "connector");
block("uart", ["J3"], "connector");
block("buttons", ["SW1", "SW2", "SW3", "SW4"], "connector", null, {
  allowDisconnected: true,
});

component("U1").block("mcu").role("main_ic").top();
component("R1").block("mcu").role("passive").top();
component("R2").block("mcu").role("passive").top();
component("C4").block("mcu_decoupling").role("decoupling_cap").top();
component("C5").block("mcu_decoupling").role("decoupling_cap").top();

component("U2").block("regulator").role("main_ic").top();
component("C1").block("regulator").role("decoupling_cap").top();
component("C2").block("regulator_out").role("decoupling_cap").top();
component("C3").block("regulator_out").role("decoupling_cap").top();

component("J1").block("battery").role("connector").top().edgePlace("right", {
  inset: 1.5,
  face: "any",
  y: 14,
});
component("J2").block("battery").role("connector").top().edgePlace("right", {
  inset: 1.5,
  face: "any",
  y: 2,
});
component("J6").block("l298_io").role("connector").top().edgePlace("right", {
  inset: 1.5,
  face: "any",
  y: -14,
});
component("J5").block("sensor").role("connector").top().edgePlace("top", {
  inset: 1.5,
  face: "any",
  x: 12,
});
component("J4").block("oled").role("connector").top().edgePlace("top", {
  inset: 1.5,
  face: "any",
  x: -28,
});
component("J3").block("uart").role("connector").top().edgePlace("left", {
  inset: 1.5,
  face: "any",
  y: 0,
});
component("SW1").block("buttons").role("connector").top().edgePlace("bottom", {
  inset: 2,
  face: "any",
  x: -32,
});
component("SW2").block("buttons").role("connector").top().edgePlace("bottom", {
  inset: 2,
  face: "any",
  x: -20,
});
component("SW3").block("buttons").role("connector").top().edgePlace("bottom", {
  inset: 2,
  face: "any",
  x: -8,
});
component("SW4").block("buttons").role("connector").top().edgePlace("bottom", {
  inset: 2,
  face: "any",
  x: 4,
});

// Battery feed into the L298N 12 V pin. Short and wide at routing time.
criticalPair(pin("J1", "1"), pin("J2", "1"), {
  maxDistance: 18,
  priority: "critical",
});

// AMS1117 input capacitor on the 5 V pin from the L298N regulator.
criticalPair(pin("C1", "1"), pin("U2", "3"), {
  maxDistance: 6,
  priority: "critical",
  preferFacingPads: true,
});
near(pin("J2", "3"), pin("U2", "3"), "high");

capCluster(["C2", "C3"], {
  powerNet: "+3V3",
  returnNet: "GND",
  target: pin("U2", "2"),
  maxRows: 1,
  gap: 0.4,
  priority: "high",
});

capCluster(["C4", "C5"], {
  powerNet: "+3V3",
  returnNet: "GND",
  target: pin("U1", "2"),
  maxRows: 1,
  gap: 0.4,
  priority: "critical",
});

solver({
  grid: 0.5,
  ignoredSignals: ["GND"],
  compactness: "normal",
});
