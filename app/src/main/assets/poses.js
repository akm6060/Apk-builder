// Pose & exercise library. Angles in degrees, model space: X = model's left, Y = up, Z = forward.
// la/ra: arm {f: flexion fwd, a: abduction, e: elbow flex, u: forearm lift (coronal), t: humeral ext. rotation, s: shoulder shrug, r: scapular retraction}
// ll/rl: leg {f: hip flexion (absolute), a: abduction, r: external rotation, k: knee flexion, an: ankle plantarflex(+)/dorsiflex(-)}
// trunk: hinge at hips (legs compensated). hips: whole-body [x,y,z]. spine/neck: [flex, turnLeft, sideBendRight].
export function pose(o = {}) {
  const b = {};
  const set = (k, v) => { b[k] = [v[0] || 0, v[1] || 0, v[2] || 0]; };
  const t = o.trunk || 0, h = o.hips || [0, 0, 0];
  set('Hips', [h[0] + t, h[1], h[2]]);
  const sp = o.spine || [0, 0, 0];
  set('Spine', [sp[0] * .3, sp[1] * .3, sp[2] * .3]); set('Spine1', [sp[0] * .35, sp[1] * .35, sp[2] * .35]); set('Spine2', [sp[0] * .35, sp[1] * .35, sp[2] * .35]);
  const nk = o.neck || [0, 0, 0];
  set('Neck', [nk[0] * .45, nk[1] * .45, nk[2] * .45]); set('Head', [nk[0] * .55, nk[1] * .55, nk[2] * .55]);
  const L = Object.assign({ f: 0, a: 7, e: 12, u: 0, t: 0, s: 0, r: 0 }, o.la), R = Object.assign({ f: 0, a: 7, e: 12, u: 0, t: 0, s: 0, r: 0 }, o.ra);
  set('LeftShoulder', [0, L.r, L.s]); set('RightShoulder', [0, -R.r, -R.s]);
  set('LeftArm', [-L.f, L.t, L.a - 90]); set('RightArm', [-R.f, -R.t, 90 - R.a]);
  set('LeftForeArm', [0, -L.e, L.u]); set('RightForeArm', [0, R.e, -R.u]);
  const LL = Object.assign({ f: 0, a: 3, r: 0, k: 0, an: 0 }, o.ll), RL = Object.assign({ f: 0, a: 3, r: 0, k: 0, an: 0 }, o.rl);
  set('LeftUpLeg', [-(LL.f + t), LL.r, LL.a]); set('RightUpLeg', [-(RL.f + t), -RL.r, -RL.a]);
  set('LeftLeg', [LL.k, 0, 0]); set('RightLeg', [RL.k, 0, 0]);
  set('LeftFoot', [LL.an, 0, 0]); set('RightFoot', [RL.an, 0, 0]);
  return { b, lift: o.lift || 0, mouth: o.mouth || 0, tongue: o.tongue || 0, ground: o.ground !== false };
}
const M = (...objs) => { // deep-ish merge of pose option objects
  const r = {};
  for (const o of objs) for (const k in o) {
    if (o[k] && typeof o[k] === 'object' && !Array.isArray(o[k])) r[k] = Object.assign({}, r[k] || {}, o[k]);
    else r[k] = o[k];
  }
  return r;
};
// ---- base option sets ----
const STAND = {};
const SUPINE = { hips: [-90, 0, 0], la: { a: 12, e: 6 }, ra: { a: 12, e: 6 }, ll: { an: 15 }, rl: { an: 15 } };
const HOOK = M(SUPINE, { ll: { f: 50, k: 95, an: 10, a: 4 }, rl: { f: 50, k: 95, an: 10, a: 4 } });
const PRONE = { hips: [90, 0, 0], la: { a: 14, e: 6 }, ra: { a: 14, e: 6 }, ll: { an: 55 }, rl: { an: 55 }, neck: [-12, 0, 0] };
const QUAD = { hips: [76, 0, 0], la: { f: 76, a: 6, e: 0 }, ra: { f: 76, a: 6, e: 0 }, ll: { f: 76, k: 90, an: 50, a: 4 }, rl: { f: 76, k: 90, an: 50, a: 4 }, neck: [-28, 0, 0] };
const SIT = { spine: [6, 0, 0], neck: [-4, 0, 0], ll: { f: 90, k: 90, a: 6 }, rl: { f: 90, k: 90, a: 6 }, la: { f: 35, e: 45, a: 8 }, ra: { f: 35, e: 45, a: 8 } };
const PLANK = { hips: [72, 0, 0], la: { f: 72, a: 10, e: 0 }, ra: { f: 72, a: 10, e: 0 }, ll: { an: -10 }, rl: { an: -10 }, neck: [-20, 0, 0] };
const F = (d, o, extra = {}) => Object.assign({ d, p: pose(o) }, extra);

// ---- basic commands ----
export const BASIC = {
  stand: { name: 'Stand', hold: true, frames: [F(1, STAND)] },
  sit: { name: 'Sit on stool', hold: true, props: ['stool'], frames: [F(1, SIT)] },
  sitfloor: { name: 'Sit on floor', hold: true, frames: [F(1.4, { hips: [-8, 0, 0], spine: [14, 0, 0], neck: [6, 0, 0], ll: { f: 82, an: 10, a: 6 }, rl: { f: 82, an: 10, a: 6 }, la: { f: -12, a: 22, e: 8 }, ra: { f: -12, a: 22, e: 8 } })], view: 'high' },
  kneel: { name: 'Kneel', hold: true, frames: [F(1, { ll: { k: 90, an: 45 }, rl: { k: 90, an: 45 } })] },
  walk: { name: 'Walk', proc: 'walk', move: 0.75 },
  run: { name: 'Run', proc: 'run', move: 2.0 },
  jump: {
    name: 'Jump', reps: 0, frames: [
      F(.35, STAND),
      F(.45, { trunk: 25, spine: [12, 0, 0], ll: { f: 55, k: 95, an: -25 }, rl: { f: 55, k: 95, an: -25 }, la: { f: -45, e: 15 }, ra: { f: -45, e: 15 } }),
      F(.25, { ll: { an: 40 }, rl: { an: 40 }, la: { f: 165, e: 5 }, ra: { f: 165, e: 5 }, lift: .32, neck: [-8, 0, 0] }),
      F(.2, { ll: { f: 10, k: 25, an: 25 }, rl: { f: 10, k: 25, an: 25 }, la: { f: 150, e: 5 }, ra: { f: 150, e: 5 }, lift: .4 }),
      F(.25, { ll: { an: 20 }, rl: { an: 20 }, la: { f: 60 }, ra: { f: 60 }, lift: 0 }),
      F(.35, { trunk: 20, spine: [10, 0, 0], ll: { f: 45, k: 80, an: -20 }, rl: { f: 45, k: 80, an: -20 }, la: { f: 40, e: 20 }, ra: { f: 40, e: 20 } }),
      F(.5, STAND)]
  },
  wave: {
    name: 'Wave hello', reps: 0, frames: [
      F(.4, { ra: { a: 95, u: 80, e: 0 }, neck: [0, -8, 0] }),
      F(.35, { ra: { a: 95, u: 80, e: 0, f: 0 }, la: {} , spine: [0, -4, 0]}, { ra2: 1 }),
      F(.35, { ra: { a: 80, u: 110, e: 0 }, spine: [0, -4, 0] })]
  },
  namaste: { name: 'Namaste', hold: true, frames: [F(1, { la: { f: 30, a: 10, e: 95, t: -55 }, ra: { f: 30, a: 10, e: 95, t: -55 }, neck: [12, 0, 0], spine: [6, 0, 0] })] },
  turn: { name: 'Turn around', proc: 'turn' },
  lieback: { name: 'Lie on back', hold: true, view: 'high', frames: [F(1.6, SUPINE)] },
  liefront: { name: 'Lie on abdomen', hold: true, view: 'high', frames: [F(1.6, PRONE)] },
  lieside: { name: 'Lie on side', hold: true, view: 'high', frames: [F(1.6, { hips: [0, 0, -88], la: { a: 10 }, ra: { f: 80, e: 30 }, ll: { f: 30, k: 40 }, rl: { f: 40, k: 50 }, neck: [0, 0, 10] })] },
  bend: { name: 'Bend forward', hold: true, frames: [F(1.4, { trunk: 75, spine: [25, 0, 0], neck: [10, 0, 0], la: { f: 80, e: 5 }, ra: { f: 80, e: 5 }, ll: { an: 0 }, rl: { an: 0 } })] },
  backbend: { name: 'Bend backward', hold: true, frames: [F(1.2, { spine: [-25, 0, 0], neck: [-20, 0, 0], la: { f: -20, e: 70 }, ra: { f: -20, e: 70 } })] },
  hands: { name: 'Hands up', hold: true, frames: [F(1, { la: { f: 178, a: 5, e: 0 }, ra: { f: 178, a: 5, e: 0 } })] },
  tpose: { name: 'Arms out to sides', hold: true, frames: [F(1, { la: { a: 90, e: 0 }, ra: { a: 90, e: 0 } })] },
  squat: { name: 'Squat down', hold: true, frames: [F(1.2, { trunk: 40, spine: [10, 0, 0], ll: { f: 95, k: 115, an: -30, a: 10 }, rl: { f: 95, k: 115, an: -30, a: 10 }, la: { f: 90, e: 0 }, ra: { f: 90, e: 0 } })] },
  quad: { name: 'On hands & knees', hold: true, view: 'high', frames: [F(1.6, QUAD)] },
  standleft: { name: 'Stand on left leg', hold: true, frames: [F(1, { rl: { f: 40, k: 70 }, la: { a: 40 }, ra: { a: 40 } })] },
  // oral
  openmouth: { name: 'Open mouth', hold: true, inset: 'face', face: { jawOpen: .85 }, frames: [F(.8, { neck: [-8, 0, 0], mouth: 1 })] },
  tongue: { name: 'Show tongue', hold: true, inset: 'face', face: { jawOpen: .55, tongueOut: 1 }, frames: [F(.8, { neck: [-5, 0, 0], mouth: .7 })] },
  tonsils: { name: 'Show tonsils – say "Aaah"', hold: true, inset: 'oral', face: { jawOpen: 1 }, frames: [F(.8, { neck: [-18, 0, 0], mouth: 1 })] },
  closemouth: { name: 'Close mouth', hold: true, frames: [F(.6, STAND)] },
};

// ---- exercises ----
const EX = [];
const ex = (id, name, cats, steps, frames, opts = {}) => EX.push(Object.assign({ id, name, cats, steps, frames, reps: 8 }, opts));

// ---------- BACK PAIN / LUMBAR ----------
ex('kneechest', 'Single knee-to-chest', ['back', 'lumbar', 'sciatica'],
  ['Lie on your back, knees bent, feet flat.', 'Pull one knee towards your chest with both hands.', 'Hold 5–10 seconds, lower slowly. Change legs.'],
  [F(1.2, HOOK, { hold: .3 }),
  F(1.4, M(HOOK, { ll: { f: 125, k: 130 }, la: { f: 55, e: 70, a: 10 }, ra: { f: 55, e: 70, a: 10 }, neck: [12, 0, 0] }), { hold: 1.2 }),
  F(1.2, HOOK, { hold: .3 }),
  F(1.4, M(HOOK, { rl: { f: 125, k: 130 }, la: { f: 55, e: 70, a: 10 }, ra: { f: 55, e: 70, a: 10 }, neck: [12, 0, 0] }), { hold: 1.2 })],
  { view: 'high', reps: 5 });
ex('doubleknee', 'Double knee-to-chest', ['back', 'lumbar'],
  ['Lie on your back.', 'Bring both knees to your chest, hug them gently.', 'Hold 10 seconds, relax.'],
  [F(1.2, HOOK, { hold: .4 }), F(1.6, M(HOOK, { ll: { f: 120, k: 130 }, rl: { f: 120, k: 130 }, la: { f: 60, e: 80 }, ra: { f: 60, e: 80 }, neck: [15, 0, 0], spine: [12, 0, 0] }), { hold: 1.5 })],
  { view: 'high', reps: 5 });
ex('pelvictilt', 'Pelvic tilt', ['back', 'lumbar'],
  ['Lie on your back, knees bent.', 'Tighten tummy, press low back flat into the floor.', 'Hold 5 seconds, relax.'],
  [F(1.2, HOOK, { hold: .5 }), F(1.2, M(HOOK, { hips: [-98, 0, 0], ll: { f: 58 }, rl: { f: 58 }, spine: [8, 0, 0] }), { hold: 1.5 })],
  { view: 'high', reps: 10 });
ex('bridge', 'Glute bridge', ['back', 'lumbar', 'gym'],
  ['Lie on back, knees bent, arms by side.', 'Squeeze buttocks and lift hips until body is straight from shoulders to knees.', 'Hold 3 seconds, lower slowly.'],
  [F(1.2, HOOK, { hold: .3 }), F(1.4, M(HOOK, { hips: [-122, 0, 0], ll: { f: -10, k: 108, an: 5 }, rl: { f: -10, k: 108, an: 5 }, neck: [28, 0, 0], la: { a: 15, f: -10 }, ra: { a: 15, f: -10 } }), { hold: 1.5 })],
  { view: 'side', reps: 10 });
ex('trunkrot', 'Lower trunk rotation', ['back', 'lumbar'],
  ['Lie on back, knees bent and together.', 'Slowly let both knees fall to one side, shoulders stay down.', 'Return to centre and go to the other side.'],
  [F(1.2, HOOK), F(1.5, M(HOOK, { ll: { a: 45 }, rl: { a: -35 }, spine: [0, -12, 0], la: { a: 45 }, ra: { a: 45 } }), { hold: 1 }), F(1.2, HOOK),
  F(1.5, M(HOOK, { ll: { a: -35 }, rl: { a: 45 }, spine: [0, 12, 0], la: { a: 45 }, ra: { a: 45 } }), { hold: 1 })],
  { view: 'high', reps: 6 });
ex('catcow', 'Cat – Cow', ['back', 'lumbar'],
  ['Get on hands and knees.', 'Cat: round your back up, tuck chin.', 'Cow: let tummy sag, lift head gently.'],
  [F(1.4, M(QUAD, { spine: [24, 0, 0], neck: [2, 0, 0], hips: [80, 0, 0], ll: { f: 80 }, rl: { f: 80 }, la: { f: 64 }, ra: { f: 64 } }), { hold: .6 }),
  F(1.4, M(QUAD, { spine: [-22, 0, 0], neck: [-45, 0, 0], hips: [72, 0, 0], ll: { f: 72 }, rl: { f: 72 }, la: { f: 92 }, ra: { f: 92 } }), { hold: .6 })],
  { view: 'side', reps: 8 });
ex('child', "Child's pose", ['back', 'lumbar'],
  ['From hands and knees, sit back on your heels.', 'Stretch arms forward, forehead towards the floor.', 'Breathe slowly and hold 20–30 seconds.'],
  [F(1.4, QUAD, { hold: .5 }), F(2.4, { hips: [123, 0, 0], spine: [8, 0, 0], neck: [12, 0, 0], ll: { f: 177, k: 145, an: 55, a: 12 }, rl: { f: 177, k: 145, an: 55, a: 12 }, la: { f: 205, a: 12, e: 0 }, ra: { f: 205, a: 12, e: 0 } }, { hold: 4 })],
  { view: 'side', reps: 3 });
ex('mckenzie', 'Prone press-up (McKenzie)', ['back', 'lumbar', 'sciatica'],
  ['Lie on your abdomen, hands under shoulders.', 'Push upper body up, keep hips on the floor, back relaxed.', 'Hold 2 seconds, lower slowly.'],
  [F(1.2, M(PRONE, { la: { f: 18, a: 28, e: 130 }, ra: { f: 18, a: 28, e: 130 } }), { hold: .4 }),
  F(1.6, M(PRONE, { spine: [-62, 0, 0], neck: [-12, 0, 0], la: { f: 24, a: 14, e: 8 }, ra: { f: 24, a: 14, e: 8 } }), { hold: 1.5 })],
  { view: 'side', reps: 10 });
ex('birddog', 'Bird-dog', ['back', 'lumbar', 'gym'],
  ['On hands and knees, back flat.', 'Stretch out one arm forward and opposite leg backward.', 'Hold 5 seconds, change sides.'],
  [F(1, QUAD), F(1.4, M(QUAD, { la: { f: 168, e: 0 }, rl: { f: 10, k: 0, an: 20 } }), { hold: 1.2 }), F(1, QUAD),
  F(1.4, M(QUAD, { ra: { f: 168, e: 0 }, ll: { f: 10, k: 0, an: 20 } }), { hold: 1.2 })],
  { view: 'side', reps: 6 });
ex('crunch', 'Partial crunch / curl-up', ['back', 'gym'],
  ['Lie on back, knees bent, arms crossed on chest.', 'Tighten tummy, lift shoulders slightly off floor.', 'Hold 2 seconds, lower slowly.'],
  [F(1, M(HOOK, { la: { f: 40, e: 120, a: 5 }, ra: { f: 40, e: 120, a: 5 } })),
  F(1.1, M(HOOK, { spine: [38, 0, 0], neck: [18, 0, 0], la: { f: 40, e: 120, a: 5 }, ra: { f: 40, e: 120, a: 5 } }), { hold: .8 })],
  { view: 'side', reps: 10 });
ex('superman', 'Superman', ['back', 'gym'],
  ['Lie on abdomen, arms stretched overhead.', 'Lift arms, chest and legs a little off the floor.', 'Hold 3 seconds, lower.'],
  [F(1.2, M(PRONE, { la: { f: 175, a: 12, e: 0 }, ra: { f: 175, a: 12, e: 0 } })),
  F(1.3, M(PRONE, { spine: [-22, 0, 0], neck: [-15, 0, 0], la: { f: 195, a: 12, e: 0 }, ra: { f: 195, a: 12, e: 0 }, ll: { f: -15 }, rl: { f: -15 } }), { hold: 1.5 })],
  { view: 'side', reps: 8 });
ex('standext', 'Standing back extension', ['back', 'lumbar'],
  ['Stand, hands on lower back.', 'Gently lean backwards, look up slightly.', 'Hold 2 seconds, come back up.'],
  [F(1, { la: { f: -25, e: 100, a: 20 }, ra: { f: -25, e: 100, a: 20 } }),
  F(1.4, { spine: [-30, 0, 0], neck: [-18, 0, 0], la: { f: -25, e: 100, a: 20 }, ra: { f: -25, e: 100, a: 20 } }, { hold: 1.2 })],
  { reps: 10 });
ex('hamstring', 'Supine hamstring stretch', ['back', 'sciatica', 'knee'],
  ['Lie on back, one knee bent.', 'Raise other leg straight up, hold behind thigh.', 'Hold 20–30 seconds, change leg.'],
  [F(1.2, HOOK), F(1.8, M(HOOK, { ll: { f: 80, k: 5, an: 0 }, la: { f: 75, e: 25 }, ra: { f: 75, e: 25 }, neck: [8, 0, 0] }), { hold: 3 }),
  F(1.2, HOOK), F(1.8, M(HOOK, { rl: { f: 80, k: 5, an: 0 }, la: { f: 75, e: 25 }, ra: { f: 75, e: 25 }, neck: [8, 0, 0] }), { hold: 3 })],
  { view: 'side', reps: 3 });

// ---------- SCIATICA ----------
ex('piriformis', 'Piriformis stretch (figure-4)', ['sciatica', 'back'],
  ['Lie on back, knees bent.', 'Cross right ankle over left knee.', 'Pull left thigh towards chest until stretch is felt in right buttock. Hold 30 s.'],
  [F(1.2, HOOK), F(1.4, M(HOOK, { rl: { f: 70, a: 25, r: 70, k: 95 } })),
  F(1.6, M(HOOK, { ll: { f: 95, k: 100 }, rl: { f: 100, a: 25, r: 70, k: 95 }, la: { f: 55, e: 80 }, ra: { f: 55, e: 80 }, neck: [12, 0, 0] }), { hold: 3 })],
  { view: 'high', reps: 3 });
ex('nerveglide', 'Sciatic nerve glide (seated)', ['sciatica'],
  ['Sit upright on a chair.', 'Straighten one knee and pull toes up while looking up.', 'Then bend knee, point toes down and look down. Smooth & painless.'],
  [F(1.2, M(SIT, { neck: [25, 0, 0], spine: [15, 0, 0], ll: { an: 25 } })),
  F(1.3, M(SIT, { neck: [-25, 0, 0], ll: { k: 5, an: -20 } }), { hold: .6 })],
  { props: ['stool'], reps: 10 });
ex('kneeopp', 'Knee to opposite shoulder', ['sciatica'],
  ['Lie on back, legs straight.', 'Bend right knee and pull it gently towards your left shoulder.', 'Hold 30 seconds, change sides.'],
  [F(1.2, SUPINE), F(1.6, M(SUPINE, { rl: { f: 110, a: -25, k: 110 }, la: { f: 55, e: 75 }, ra: { f: 55, e: 75 }, neck: [10, 0, 0] }), { hold: 2.5 }),
  F(1.2, SUPINE), F(1.6, M(SUPINE, { ll: { f: 110, a: -25, k: 110 }, la: { f: 55, e: 75 }, ra: { f: 55, e: 75 }, neck: [10, 0, 0] }), { hold: 2.5 })],
  { view: 'high', reps: 3 });
ex('pronehip', 'Prone hip extension', ['sciatica', 'back'],
  ['Lie on abdomen, forehead on hands.', 'Tighten buttock, lift one straight leg a little.', 'Hold 3 seconds, lower. Alternate.'],
  [F(1, M(PRONE, { la: { a: 75, e: 120 }, ra: { a: 75, e: 120 }, neck: [0, 0, 0] })),
  F(1.2, M(PRONE, { la: { a: 75, e: 120 }, ra: { a: 75, e: 120 }, neck: [0, 0, 0], ll: { f: -22 } }), { hold: 1 }),
  F(1, M(PRONE, { la: { a: 75, e: 120 }, ra: { a: 75, e: 120 }, neck: [0, 0, 0] })),
  F(1.2, M(PRONE, { la: { a: 75, e: 120 }, ra: { a: 75, e: 120 }, neck: [0, 0, 0], rl: { f: -22 } }), { hold: 1 })],
  { view: 'side', reps: 6 });

// ---------- NECK / CERVICAL ----------
const SITN = M(SIT, { la: { f: 30, e: 50 }, ra: { f: 30, e: 50 } });
ex('chintuck', 'Chin tuck', ['neck'],
  ['Sit tall, look straight ahead.', 'Glide your chin straight back (make a double chin).', 'Hold 5 seconds, relax.'],
  [F(1, SITN), F(1, M(SITN, { neck: [-14, 0, 0], spine: [0, 0, 0] }), { neckTuck: 1, hold: 1.5 })],
  { props: ['stool'], reps: 10, cam: 'head' });
ex('neckside', 'Neck side bend', ['neck'],
  ['Sit tall, shoulders relaxed.', 'Tilt ear towards shoulder until gentle stretch.', 'Hold 15 seconds each side.'],
  [F(1, SITN), F(1.4, M(SITN, { neck: [0, 0, 38] }), { hold: 2 }), F(1, SITN), F(1.4, M(SITN, { neck: [0, 0, -38] }), { hold: 2 })],
  { props: ['stool'], reps: 4, cam: 'head' });
ex('neckrot', 'Neck rotation', ['neck'],
  ['Sit tall.', 'Slowly turn head to look over one shoulder.', 'Hold 5 seconds, turn to the other side.'],
  [F(1, SITN), F(1.4, M(SITN, { neck: [0, 65, 0] }), { hold: 1.5 }), F(1, SITN), F(1.4, M(SITN, { neck: [0, -65, 0] }), { hold: 1.5 })],
  { props: ['stool'], reps: 5, cam: 'head' });
ex('neckflex', 'Neck flexion – extension', ['neck'],
  ['Sit tall.', 'Bring chin down towards chest, hold.', 'Then look up gently towards ceiling, hold.'],
  [F(1.3, M(SITN, { neck: [45, 0, 0] }), { hold: 1.5 }), F(1.6, M(SITN, { neck: [-35, 0, 0] }), { hold: 1.5 })],
  { props: ['stool'], reps: 5, cam: 'head' });
ex('shrug', 'Shoulder shrugs', ['neck', 'shoulder', 'gym'],
  ['Stand with arms by your side.', 'Lift shoulders up towards ears.', 'Hold 2 seconds, roll back and down.'],
  [F(.8, STAND), F(.9, { la: { s: 18, a: 9 }, ra: { s: 18, a: 9 } }, { hold: .8 })],
  { reps: 10, cam: 'upper' });
ex('scapsqueeze', 'Shoulder blade squeeze', ['neck', 'shoulder'],
  ['Stand tall, elbows bent at 90°.', 'Squeeze shoulder blades together, pull elbows back.', 'Hold 5 seconds.'],
  [F(1, { la: { a: 25, e: 90 }, ra: { a: 25, e: 90 } }), F(1.2, { la: { a: 30, e: 90, f: -25, r: 15 }, ra: { a: 30, e: 90, f: -25, r: 15 }, spine: [-5, 0, 0] }, { hold: 1.5 })],
  { reps: 10, cam: 'upper' });

// ---------- CALF / LEG / KNEE ----------
ex('calfraise', 'Calf raises', ['calf', 'gym'],
  ['Stand straight, feet hip width.', 'Rise up on your toes as high as possible.', 'Hold 2 seconds, lower slowly.'],
  [F(.9, STAND), F(1, { ll: { an: 38 }, rl: { an: 38 }, la: { a: 12 }, ra: { a: 12 } }, { hold: .8 })],
  { reps: 12, cam: 'legs' });
ex('calfstretch', 'Wall calf stretch', ['calf'],
  ['Hands on the wall, one foot forward.', 'Keep back leg straight with heel on floor.', 'Lean forward until calf stretch. Hold 30 s, change leg.'],
  [F(1.4, { trunk: 12, ll: { f: 30, k: 35, an: -15 }, rl: { f: -22, an: -18 }, la: { f: 85, e: 25 }, ra: { f: 85, e: 25 } }, { hold: 3 }),
  F(1.4, { trunk: 12, rl: { f: 30, k: 35, an: -15 }, ll: { f: -22, an: -18 }, la: { f: 85, e: 25 }, ra: { f: 85, e: 25 } }, { hold: 3 })],
  { reps: 3, cam: 'side' });
ex('anklepump', 'Ankle pumps', ['calf', 'knee'],
  ['Lie on back, legs straight.', 'Pull toes up towards you.', 'Then point toes away. Keep a steady rhythm.'],
  [F(.8, M(SUPINE, { ll: { an: -25 }, rl: { an: -25 } })), F(.8, M(SUPINE, { ll: { an: 45 }, rl: { an: 45 } }))],
  { view: 'high', reps: 15, cam: 'feet' });
ex('slr', 'Straight leg raise', ['knee', 'back'],
  ['Lie on back, one knee bent.', 'Tighten thigh of straight leg, lift it to the height of the other knee.', 'Hold 3 s, lower slowly. Change leg.'],
  [F(1, M(SUPINE, { rl: { f: 50, k: 95 } })), F(1.3, M(SUPINE, { rl: { f: 50, k: 95 }, ll: { f: 45, an: -10 } }), { hold: 1.2 }),
  F(1, M(SUPINE, { ll: { f: 50, k: 95 } })), F(1.3, M(SUPINE, { ll: { f: 50, k: 95 }, rl: { f: 45, an: -10 } }), { hold: 1.2 })],
  { view: 'side', reps: 6 });
ex('quadstretch', 'Standing quadriceps stretch', ['knee', 'calf'],
  ['Stand, hold support if needed.', 'Bend one knee, hold ankle behind you.', 'Keep knees together, hold 20 s. Change leg.'],
  [F(1.4, { ll: { f: -8, k: 150, an: 30 }, la: { f: -35, e: 50 }, ra: { a: 40 } }, { hold: 2.5 }), F(1, STAND),
  F(1.4, { rl: { f: -8, k: 150, an: 30 }, ra: { f: -35, e: 50 }, la: { a: 40 } }, { hold: 2.5 }), F(1, STAND)],
  { reps: 3, cam: 'side' });
ex('heelslide', 'Heel slides', ['knee'],
  ['Lie on back, legs straight.', 'Slide one heel towards buttock, bending the knee.', 'Slide back. Alternate legs.'],
  [F(1.2, SUPINE), F(1.4, M(SUPINE, { ll: { f: 70, k: 110 } })), F(1.2, SUPINE), F(1.4, M(SUPINE, { rl: { f: 70, k: 110 } }))],
  { view: 'high', reps: 6 });

// ---------- SHOULDER ----------
ex('armcircle', 'Arm circles', ['shoulder', 'gym'],
  ['Stand, arms straight out to the sides.', 'Make small circles forward.', 'Then circle backward.'],
  [F(.4, { la: { a: 90, e: 0, f: 0 }, ra: { a: 90, e: 0, f: 0 } }), F(.4, { la: { a: 100, e: 0, f: 12 }, ra: { a: 100, e: 0, f: 12 } }),
  F(.4, { la: { a: 90, e: 0, f: 22 }, ra: { a: 90, e: 0, f: 22 } }), F(.4, { la: { a: 80, e: 0, f: 12 }, ra: { a: 80, e: 0, f: 12 } })],
  { reps: 10, cam: 'upper' });
ex('pendulum', 'Pendulum (Codman)', ['shoulder'],
  ['Lean forward, support with good hand on a table.', 'Let the painful arm hang loosely.', 'Swing it gently in small circles.'],
  [F(.8, { trunk: 60, la: { f: 75, e: 70, a: 25 }, ra: { f: 60, e: 0, a: 4 }, ll: { k: 15 }, rl: { k: 15 } }),
  F(.8, { trunk: 60, la: { f: 75, e: 70, a: 25 }, ra: { f: 70, e: 0, a: 14 }, ll: { k: 15 }, rl: { k: 15 } }),
  F(.8, { trunk: 60, la: { f: 75, e: 70, a: 25 }, ra: { f: 60, e: 0, a: 24 }, ll: { k: 15 }, rl: { k: 15 } }),
  F(.8, { trunk: 60, la: { f: 75, e: 70, a: 25 }, ra: { f: 50, e: 0, a: 14 }, ll: { k: 15 }, rl: { k: 15 } })],
  { reps: 8, cam: 'side' });
ex('wallclimb', 'Finger wall walk (arm raise)', ['shoulder'],
  ['Face a wall, fingers on wall at waist level.', 'Walk fingers slowly up the wall as high as comfortable.', 'Hold, then walk back down.'],
  [F(1.4, { ra: { f: 70, e: 20 } }), F(2.2, { ra: { f: 170, e: 0 }, spine: [-4, 0, 0] }, { hold: 1.5 })],
  { reps: 5, cam: 'side' });

// ---------- BODY BUILDING / GYM ----------
ex('squat', 'Bodyweight squats', ['gym', 'knee'],
  ['Stand, feet shoulder width, arms forward.', 'Push hips back and bend knees until thighs are near horizontal.', 'Push through heels to stand.'],
  [F(.9, { la: { f: 90, e: 0 }, ra: { f: 90, e: 0 } }), F(1.1, { trunk: 40, spine: [8, 0, 0], ll: { f: 92, k: 112, an: -30, a: 10 }, rl: { f: 92, k: 112, an: -30, a: 10 }, la: { f: 90, e: 0 }, ra: { f: 90, e: 0 } }, { hold: .3 })],
  { reps: 12, cam: 'side' });
ex('lunge', 'Forward lunges', ['gym', 'knee'],
  ['Stand tall, hands on hips.', 'Step one leg forward, lower back knee towards floor.', 'Push back up, alternate legs.'],
  [F(.8, { la: { a: 30, e: 100 }, ra: { a: 30, e: 100 } }), F(1.1, { ll: { f: 80, k: 90, an: -5 }, rl: { f: -15, k: 95, an: 35 }, la: { a: 30, e: 100 }, ra: { a: 30, e: 100 } }, { hold: .4 }),
  F(.8, { la: { a: 30, e: 100 }, ra: { a: 30, e: 100 } }), F(1.1, { rl: { f: 80, k: 90, an: -5 }, ll: { f: -15, k: 95, an: 35 }, la: { a: 30, e: 100 }, ra: { a: 30, e: 100 } }, { hold: .4 })],
  { reps: 8, cam: 'side' });
ex('pushup', 'Push-ups', ['gym', 'shoulder'],
  ['Hands under shoulders, body straight like a plank.', 'Lower chest towards floor, elbows at 45°.', 'Push back up.'],
  [F(.9, PLANK, { hold: .2 }), F(1, M(PLANK, { hips: [84, 0, 0], la: { f: 8, a: 35, e: 82 }, ra: { f: 8, a: 35, e: 82 } }), { hold: .2 })],
  { view: 'side', reps: 10 });
ex('plank', 'Forearm plank (hold)', ['gym', 'back'],
  ['Forearms on floor, elbows under shoulders.', 'Body straight from head to heels, tummy tight.', 'Hold 20–60 seconds, keep breathing.'],
  [F(1.6, { hips: [80, 0, 0], la: { f: 82, e: 95, a: 6 }, ra: { f: 82, e: 95, a: 6 }, ll: { an: -10 }, rl: { an: -10 }, neck: [-10, 0, 0] }, { hold: 6 })],
  { view: 'side', reps: 2 });
ex('jacks', 'Jumping jacks', ['gym'],
  ['Stand, arms by side.', 'Jump feet apart while raising arms overhead.', 'Jump back. Keep a rhythm.'],
  [F(.3, { lift: .06, la: { a: 90, e: 0 }, ra: { a: 90, e: 0 }, ll: { a: 12, an: 20 }, rl: { a: 12, an: 20 } }),
  F(.25, { la: { a: 170, e: 0 }, ra: { a: 170, e: 0 }, ll: { a: 20 }, rl: { a: 20 } }),
  F(.3, { lift: .06, la: { a: 90, e: 0 }, ra: { a: 90, e: 0 }, ll: { a: 12, an: 20 }, rl: { a: 12, an: 20 } }),
  F(.25, STAND)],
  { reps: 15 });
ex('curl', 'Biceps curls (dumbbells)', ['gym'],
  ['Stand, dumbbells in hands, palms forward.', 'Bend elbows, curl weights to shoulders.', 'Lower slowly, keep elbows at side.'],
  [F(.9, { la: { a: 8, e: 5, t: 30 }, ra: { a: 8, e: 5, t: 30 } }), F(1, { la: { a: 8, e: 135, t: 30 }, ra: { a: 8, e: 135, t: 30 } }, { hold: .3 })],
  { props: ['dumbbell'], reps: 12, cam: 'upper' });
ex('press', 'Shoulder press (dumbbells)', ['gym', 'shoulder'],
  ['Hold dumbbells at shoulder level.', 'Press them straight overhead.', 'Lower slowly to shoulders.'],
  [F(.9, { la: { f: 55, a: 25, e: 125 }, ra: { f: 55, a: 25, e: 125 } }), F(1, { la: { f: 178, a: 10, e: 0 }, ra: { f: 178, a: 10, e: 0 } }, { hold: .3 })],
  { props: ['dumbbell'], reps: 10, cam: 'upper' });
ex('lateral', 'Lateral raises (dumbbells)', ['gym', 'shoulder'],
  ['Stand, dumbbells at your sides.', 'Raise arms out to the side to shoulder height.', 'Lower slowly.'],
  [F(.9, { la: { a: 10, e: 10 }, ra: { a: 10, e: 10 } }), F(1.1, { la: { a: 88, e: 12 }, ra: { a: 88, e: 12 } }, { hold: .4 })],
  { props: ['dumbbell'], reps: 10, cam: 'upper' });
ex('situp', 'Sit-ups', ['gym'],
  ['Lie on back, knees bent, hands across chest.', 'Curl all the way up to sitting.', 'Lower back down with control.'],
  [F(1, M(HOOK, { la: { f: 40, e: 120 }, ra: { f: 40, e: 120 } })), F(1.2, M(HOOK, { hips: [-30, 0, 0], ll: { f: 110, k: 100 }, rl: { f: 110, k: 100 }, spine: [25, 0, 0], neck: [10, 0, 0], la: { f: 40, e: 120 }, ra: { f: 40, e: 120 } }), { hold: .3 })],
  { view: 'side', reps: 10 });
ex('climber', 'Mountain climbers', ['gym'],
  ['Start in high plank.', 'Drive one knee towards chest.', 'Switch legs quickly, keep hips low.'],
  [F(.4, M(PLANK, { ll: { f: 95, k: 110 } })), F(.4, M(PLANK, { rl: { f: 95, k: 110 } }))],
  { view: 'side', reps: 16 });
ex('hinge', 'Hip hinge / good morning', ['gym', 'back'],
  ['Stand, hands behind head, knees soft.', 'Push hips back, bend forward with a straight back.', 'Squeeze buttocks to stand up.'],
  [F(1, { la: { a: 75, e: 145, f: 10 }, ra: { a: 75, e: 145, f: 10 } }), F(1.3, { trunk: 65, ll: { k: 15 }, rl: { k: 15 }, la: { a: 75, e: 145, f: 10 }, ra: { a: 75, e: 145, f: 10 }, neck: [-10, 0, 0] }, { hold: .4 })],
  { reps: 10, cam: 'side' });
ex('burpee', 'Burpees', ['gym'],
  ['Squat, hands on floor.', 'Jump feet back to plank.', 'Jump feet in and jump up with arms overhead.'],
  [F(.5, STAND),
  F(.5, { trunk: 55, spine: [20, 0, 0], ll: { f: 100, k: 125, an: -25 }, rl: { f: 100, k: 125, an: -25 }, la: { f: 60, e: 0 }, ra: { f: 60, e: 0 } }),
  F(.5, PLANK), F(.5, { trunk: 55, spine: [20, 0, 0], ll: { f: 100, k: 125, an: -25 }, rl: { f: 100, k: 125, an: -25 }, la: { f: 60, e: 0 }, ra: { f: 60, e: 0 } }),
  F(.35, { la: { f: 170, e: 0 }, ra: { f: 170, e: 0 }, ll: { an: 35 }, rl: { an: 35 }, lift: .3 }), F(.4, STAND)],
  { reps: 6, cam: 'side' });

export const EXERCISES = EX;
export const CATEGORIES = [
  { id: 'basic', name: 'Commands', hi: 'आदेश' },
  { id: 'oral', name: 'Mouth', hi: 'मुंह' },
  { id: 'back', name: 'Backache', hi: 'पीठ दर्द' },
  { id: 'lumbar', name: 'Lumbar', hi: 'कमर' },
  { id: 'sciatica', name: 'Sciatica', hi: 'साइटिका' },
  { id: 'neck', name: 'Cervical', hi: 'गर्दन' },
  { id: 'calf', name: 'Calf', hi: 'पिंडली' },
  { id: 'knee', name: 'Knee & leg', hi: 'घुटना' },
  { id: 'shoulder', name: 'Shoulder', hi: 'कंधा' },
  { id: 'gym', name: 'Body building', hi: 'बॉडी बिल्डिंग' },
];
export const BASIC_ORDER = ['stand', 'sit', 'sitfloor', 'walk', 'run', 'jump', 'wave', 'namaste', 'turn', 'kneel', 'squat', 'bend', 'backbend', 'hands', 'tpose', 'standleft', 'quad', 'lieback', 'liefront', 'lieside'];
export const ORAL_ORDER = ['openmouth', 'tongue', 'tonsils', 'closemouth'];

// voice phrases (English, Hinglish, Hindi)
export const PHRASES = {
  stand: ['stand up', 'stand', 'get up', 'khade ho', 'khada', 'uth jao', 'खड़े हो', 'खड़े', 'खड़ा', 'उठो', 'उठ जाओ', 'खड़ी'],
  sit: ['sit down', 'sit on chair', 'sit on stool', 'sit', 'baitho', 'baith jao', 'बैठो', 'बैठ जाओ', 'बैठिए', 'बैठ', 'कुर्सी पर बैठो'],
  sitfloor: ['sit on floor', 'sit on the floor', 'zameen par baitho', 'ज़मीन पर बैठो', 'जमीन पर बैठो', 'नीचे बैठो'],
  walk: ['walk', 'walking', 'chalo', 'chaliye', 'चलो', 'चलिए', 'टहलो', 'चलना'],
  run: ['run', 'running', 'jog', 'daudo', 'दौड़ो', 'दौड़', 'भागो'],
  jump: ['jump', 'kudo', 'कूदो', 'कूद', 'उछलो'],
  wave: ['wave', 'say hello', 'hello', 'bye', 'हाथ हिलाओ', 'हेलो'],
  namaste: ['namaste', 'namaskar', 'नमस्ते', 'नमस्कार', 'प्रणाम'],
  turn: ['turn around', 'turn back', 'turn', 'ghumo', 'peeche muro', 'घूमो', 'पीछे मुड़ो', 'मुड़ो', 'घूम जाओ'],
  kneel: ['kneel', 'kneel down', 'on your knees', 'घुटनों के बल', 'घुटने टेको'],
  squat: ['squat', 'squat down', 'उकड़ू बैठो', 'उकडू'],
  bend: ['bend forward', 'touch your toes', 'touch toes', 'bend down', 'bend', 'aage jhuko', 'आगे झुको', 'झुको', 'झुक जाओ'],
  backbend: ['bend backward', 'bend back', 'lean back', 'पीछे झुको'],
  hands: ['raise your hands', 'raise hands', 'hands up', 'arms up', 'raise arms', 'haath upar', 'हाथ ऊपर', 'हाथ उठाओ', 'हाथ ऊपर करो'],
  tpose: ['arms out', 'spread arms', 'arms to the side', 'हाथ फैलाओ'],
  standleft: ['stand on one leg', 'one leg', 'balance', 'एक पैर पर खड़े'],
  quad: ['hands and knees', 'all fours', 'quadruped', 'घुटनों और हाथों'],
  lieback: ['lie on your back', 'lie on back', 'lie down on back', 'supine', 'lie down', 'lay down', 'lie flat', 'peeth ke bal', 'पीठ के बल', 'सीधे लेट', 'लेट जाओ', 'लेटो', 'लेट जाइए'],
  liefront: ['on your stomach', 'on your abdomen', 'on your tummy', 'on the stomach', 'face down', 'lie on abdomen', 'lie on your abdomen', 'lie on stomach', 'lie on your stomach', 'on your belly', 'on stomach', 'prone', 'lie on front', 'turn over', 'pet ke bal', 'पेट के बल', 'उल्टा लेट', 'उल्टे लेट'],
  lieside: ['lie on side', 'lie on your side', 'side lying', 'करवट', 'करवट लो'],
  openmouth: ['open your mouth', 'open mouth', 'open', 'muh kholo', 'munh kholo', 'मुंह खोलो', 'मुँह खोलो', 'मुंह खोलिए', 'मुँह खोलिए'],
  tongue: ['show tongue', 'show your tongue', 'stick out tongue', 'put out tongue', 'tongue', 'jeebh', 'jeebh dikhao', 'जीभ', 'जीभ दिखाओ', 'जीभ बाहर'],
  tonsils: ['show tonsils', 'show your tonsils', 'tonsils', 'tonsil', 'throat', 'say aah', 'say ah', 'aah', 'gala dikhao', 'गला दिखाओ', 'गला', 'टॉन्सिल', 'आ बोलो', 'आ करो'],
  closemouth: ['close your mouth', 'close mouth', 'shut mouth', 'muh band', 'मुंह बंद', 'मुँह बंद'],
  stop: ['stop', 'freeze', 'that is enough', 'enough', 'ruko', 'bas', 'रुको', 'रुक जाओ', 'बस', 'बंद करो'],
};
export const EX_PHRASES = {
  kneechest: ['knee to chest', 'single knee', 'घुटना छाती'], doubleknee: ['double knee', 'both knees to chest', 'दोनों घुटने'],
  pelvictilt: ['pelvic tilt'], bridge: ['bridge', 'glute bridge', 'hip raise', 'ब्रिज'], trunkrot: ['trunk rotation', 'lower trunk', 'knee rolls'],
  catcow: ['cat cow', 'cat and cow', 'cat camel', 'cat'], child: ['child pose', "child's pose", 'balasana', 'बालासन'],
  mckenzie: ['mckenzie', 'press up', 'prone press', 'cobra', 'भुजंगासन'], birddog: ['bird dog', 'bird-dog'], crunch: ['crunch', 'curl up'],
  superman: ['superman'], standext: ['standing extension', 'back extension'], hamstring: ['hamstring'],
  piriformis: ['piriformis', 'figure 4', 'figure four'], nerveglide: ['nerve glide', 'nerve floss', 'sciatic nerve'], kneeopp: ['opposite shoulder'],
  pronehip: ['hip extension', 'leg lift prone'], chintuck: ['chin tuck', 'chin in'], neckside: ['side bend', 'ear to shoulder', 'neck tilt'],
  neckrot: ['neck rotation', 'neck turn', 'look over shoulder'], neckflex: ['neck flexion', 'chin to chest', 'neck extension'],
  shrug: ['shrug', 'shoulder shrug'], scapsqueeze: ['shoulder blade', 'scapular squeeze', 'squeeze'],
  calfraise: ['calf raise', 'heel raise', 'toe raise', 'एड़ी उठाओ'], calfstretch: ['calf stretch', 'wall stretch'], anklepump: ['ankle pump', 'ankle'],
  slr: ['straight leg raise', 'leg raise'], quadstretch: ['quadriceps stretch', 'quad stretch', 'thigh stretch'], heelslide: ['heel slide'],
  armcircle: ['arm circle'], pendulum: ['pendulum', 'codman'], wallclimb: ['wall walk', 'finger walk', 'wall climb'],
  squat: ['squats', 'bodyweight squat'], lunge: ['lunge', 'lunges'], pushup: ['push up', 'push-up', 'pushups', 'push ups', 'dand', 'दंड'],
  plank: ['plank'], jacks: ['jumping jack', 'jumping jacks', 'star jump'], curl: ['bicep curl', 'biceps curl', 'curl', 'biceps'],
  press: ['shoulder press', 'overhead press', 'military press'], lateral: ['lateral raise', 'side raise'], situp: ['sit up', 'sit-up', 'situps', 'sit ups'],
  climber: ['mountain climber'], hinge: ['hip hinge', 'good morning'], burpee: ['burpee', 'burpees'],
};
export const CAT_PHRASES = {
  back: ['backache', 'back pain', 'back exercise', 'pain in back', 'back', 'kamar dard', 'peeth dard', 'पीठ दर्द', 'पीठ का दर्द', 'कमर दर्द'],
  lumbar: ['lumbar', 'low back', 'lower back', 'कमर', 'kamar'],
  sciatica: ['sciatica', 'साइटिका', 'सायटिका', 'नस का दर्द'],
  neck: ['cervical', 'neck pain', 'neck exercise', 'neck', 'gardan dard', 'spondylosis', 'गर्दन', 'gardan'],
  calf: ['calf pain', 'calf', 'पिंडली', 'pindli'],
  knee: ['knee pain', 'knee', 'घुटने का दर्द', 'घुटना'],
  shoulder: ['shoulder pain', 'frozen shoulder', 'shoulder', 'कंधा', 'कंधे'],
  gym: ['body building', 'bodybuilding', 'workout', 'gym', 'बॉडी बिल्डिंग', 'कसरत'],
};
