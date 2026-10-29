"use strict";

// Runs the game core (zentris.js / zentris.wasm) in the page: a fixed 60 Hz step like the
// desktop simulator, on-screen buttons for touch screens, and the simulator's keyboard keys.

const W = 320;
const H = 240;
const STEP_MS = 1000 / 60;

// Mirrors zentris::Action in src/core/game.h.
const Action = {
  Left: 1 << 0,
  Right: 1 << 1,
  Up: 1 << 2,
  Down: 1 << 3,
  HardDrop: 1 << 4,
  RotateCW: 1 << 5,
  RotateCCW: 1 << 6,
  Start: 1 << 7,
  Pause: 1 << 8,
};

// Same bindings as the simulator: arrows / WASD, Space drops, Z/J and X/K rotate.
const KEYS = {
  ArrowLeft: Action.Left, KeyA: Action.Left,
  ArrowRight: Action.Right, KeyD: Action.Right,
  ArrowUp: Action.Up, KeyW: Action.Up,
  ArrowDown: Action.Down, KeyS: Action.Down,
  Space: Action.HardDrop,
  KeyX: Action.RotateCW, KeyK: Action.RotateCW,
  KeyZ: Action.RotateCCW, KeyJ: Action.RotateCCW,
  Enter: Action.Start, NumpadEnter: Action.Start,
  KeyP: Action.Pause,
};

const DIRS = { up: Action.Up, down: Action.Down, left: Action.Left, right: Action.Right };

const keysHeld = new Set();
const pointers = new Map();  // pointerId -> { el, action }
// Presses since the last tick, so a tap shorter than one step still registers.
let latched = 0;

function heldMask() {
  let held = 0;
  for (const code of keysHeld) held |= KEYS[code];
  for (const p of pointers.values()) held |= p.action;
  return held;
}

function press(action) {
  latched |= action;
}

function releaseAll() {
  keysHeld.clear();
  pointers.clear();
  showPressed();
}

// --- On-screen controls ---------------------------------------------------------------

const dpad = document.getElementById("dpad");
const arms = [...dpad.querySelectorAll(".arm")];
const buttons = [...document.querySelectorAll("button[data-action]")];

function showPressed() {
  const active = new Set([...pointers.values()].map((p) => p.el));
  for (const el of buttons) el.classList.toggle("pressed", active.has(el));
  for (const el of arms) el.classList.toggle("pressed", active.has(el));
}

function buzz() {
  if (navigator.vibrate) navigator.vibrate(8);
}

for (const el of buttons) {
  const action = Action[el.dataset.action];
  el.addEventListener("pointerdown", (e) => {
    e.preventDefault();
    el.setPointerCapture(e.pointerId);
    pointers.set(e.pointerId, { el, action });
    press(action);
    buzz();
    showPressed();
  });
}

// The pad is one touch area: the direction follows the thumb as it slides between arms.
// A small dead zone in the middle; otherwise the dominant axis wins (no diagonals).
function dpadArm(e) {
  const r = dpad.getBoundingClientRect();
  const dx = (e.clientX - (r.left + r.width / 2)) / (r.width / 2);
  const dy = (e.clientY - (r.top + r.height / 2)) / (r.height / 2);
  if (Math.hypot(dx, dy) < 0.22) return null;
  const dir = Math.abs(dx) > Math.abs(dy) ? (dx < 0 ? "left" : "right") : (dy < 0 ? "up" : "down");
  return arms.find((a) => a.dataset.dir === dir);
}

function dpadUpdate(e) {
  const arm = dpadArm(e);
  const prev = pointers.get(e.pointerId);
  if (prev && prev.el === arm) return;
  if (arm) {
    const action = DIRS[arm.dataset.dir];
    pointers.set(e.pointerId, { el: arm, action });
    press(action);
    buzz();
  } else {
    pointers.set(e.pointerId, { el: null, action: 0 });
  }
  showPressed();
}

dpad.addEventListener("pointerdown", (e) => {
  e.preventDefault();
  dpad.setPointerCapture(e.pointerId);
  dpadUpdate(e);
});
dpad.addEventListener("pointermove", (e) => {
  if (pointers.has(e.pointerId)) dpadUpdate(e);
});

function pointerEnd(e) {
  if (pointers.delete(e.pointerId)) showPressed();
}
for (const type of ["pointerup", "pointercancel", "lostpointercapture"])
  document.addEventListener(type, pointerEnd, true);

// Long presses must not open menus; iOS also needs its pinch gesture blocked.
document.addEventListener("contextmenu", (e) => e.preventDefault());
document.addEventListener("gesturestart", (e) => e.preventDefault());
document.addEventListener("dblclick", (e) => e.preventDefault());

// --- Keyboard -------------------------------------------------------------------------

window.addEventListener("keydown", (e) => {
  if (!(e.code in KEYS) || e.ctrlKey || e.metaKey || e.altKey) return;
  e.preventDefault();
  if (!e.repeat) press(KEYS[e.code]);
  keysHeld.add(e.code);
});
window.addEventListener("keyup", (e) => keysHeld.delete(e.code));
window.addEventListener("blur", releaseAll);

// --- Screen ---------------------------------------------------------------------------

const box = document.getElementById("screen-box");
const canvas = document.getElementById("screen");
const ctx = canvas.getContext("2d");
const image = ctx.createImageData(W, H);
const statusLine = document.getElementById("status");

// Fills the box. Phones are too narrow to give up space for whole-pixel scaling, and at
// their pixel densities the slightly uneven pixels are hard to see.
function fitScreen() {
  const r = box.getBoundingClientRect();
  const scale = Math.min(r.width / W, r.height / H);
  canvas.style.width = `${W * scale}px`;
  canvas.style.height = `${H * scale}px`;
}

new ResizeObserver(fitScreen).observe(box);
window.addEventListener("resize", fitScreen);
fitScreen();

// --- Game loop ------------------------------------------------------------------------

createZentris().then((game) => {
  const seed = crypto.getRandomValues(new Uint32Array(1))[0];
  game._zentris_init(seed);
  statusLine.textContent = "";

  const tick = () => {
    game._zentris_tick(heldMask() | latched);
    latched = 0;
  };

  // Leaving the page (app switch, locked phone) pauses a running game.
  document.addEventListener("visibilitychange", () => {
    if (!document.hidden) return;
    releaseAll();
    if (game._zentris_playing()) {
      tick();  // flush presses still waiting for a tick
      press(Action.Pause);
      tick();
    }
  });

  let last = performance.now();
  let acc = STEP_MS;  // draw the first frame right away

  function frame(now) {
    acc = Math.min(acc + now - last, 5 * STEP_MS);  // don't spiral after a stall
    last = now;
    let stepped = false;
    while (acc >= STEP_MS) {
      tick();
      acc -= STEP_MS;
      stepped = true;
    }
    if (stepped) {
      const ptr = game._zentris_render();
      image.data.set(game.HEAPU8.subarray(ptr, ptr + W * H * 4));
      ctx.putImageData(image, 0, 0);
    }
    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);
}).catch((err) => {
  statusLine.textContent = "Could not start the game.";
  console.error(err);
});
