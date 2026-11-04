import * as THREE from 'three';
import { GLTFLoader } from './lib/examples/jsm/loaders/GLTFLoader.js';
import { MeshoptDecoder } from './lib/examples/jsm/libs/meshopt_decoder.module.js';
import { OrbitControls } from './lib/examples/jsm/controls/OrbitControls.js';
import { RoomEnvironment } from './lib/examples/jsm/environments/RoomEnvironment.js';
import { BASIC, EXERCISES, CATEGORIES, BASIC_ORDER, ORAL_ORDER, PHRASES, EX_PHRASES, CAT_PHRASES, pose } from './poses.js';
import { buildOral } from './oral.js';

const D2R = Math.PI / 180;
const $ = s => document.querySelector(s);
const NATIVE = window.AndroidBridge || null;
const EXMAP = Object.fromEntries(EXERCISES.map(e => [e.id, e]));

// ------------------------------------------------------------------ renderer / scene
const canvas = $('#c');
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, powerPreference: 'high-performance' });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.outputColorSpace = THREE.SRGBColorSpace;
renderer.toneMapping = THREE.ACESFilmicToneMapping; renderer.toneMappingExposure = 1.05;
renderer.shadowMap.enabled = true; renderer.shadowMap.type = THREE.PCFShadowMap;
renderer.autoClear = false;
const scene = new THREE.Scene();
scene.background = new THREE.Color(0xdfe5ec);
scene.fog = new THREE.Fog(0xdfe5ec, 9, 22);
const pmrem = new THREE.PMREMGenerator(renderer);
scene.environment = pmrem.fromScene(new RoomEnvironment(), .04).texture;
scene.environmentIntensity = .55;
const camera = new THREE.PerspectiveCamera(38, 1, .05, 60);
camera.position.set(0, 1.4, 6);
const controls = new OrbitControls(camera, canvas);
controls.target.set(0, .95, 0); controls.enableDamping = true; controls.maxPolarAngle = Math.PI * .495; controls.minDistance = .5; controls.maxDistance = 14;
let userCam = false; controls.addEventListener('start', () => { userCam = true; });

scene.add(new THREE.HemisphereLight(0xffffff, 0x8a7f74, 1.1));
const sun = new THREE.DirectionalLight(0xfff3e2, 2.4); sun.position.set(3, 6, 4); sun.castShadow = true;
sun.shadow.mapSize.set(1024, 1024); Object.assign(sun.shadow.camera, { left: -4, right: 4, top: 4, bottom: -4, near: .5, far: 20 }); sun.shadow.bias = -.0004; sun.shadow.normalBias = .02;
scene.add(sun);
const rim = new THREE.DirectionalLight(0xcfe2ff, 1.0); rim.position.set(-4, 3, -4); scene.add(rim);
// floor
{
  const c = document.createElement('canvas'); c.width = c.height = 512; const x = c.getContext('2d');
  const gr = x.createRadialGradient(256, 256, 30, 256, 256, 256); gr.addColorStop(0, '#f4f1ec'); gr.addColorStop(1, '#d9dde3'); x.fillStyle = gr; x.fillRect(0, 0, 512, 512);
  x.strokeStyle = 'rgba(120,130,140,.12)'; x.lineWidth = 2; for (let i = 0; i <= 512; i += 32) { x.beginPath(); x.moveTo(i, 0); x.lineTo(i, 512); x.stroke(); x.beginPath(); x.moveTo(0, i); x.lineTo(512, i); x.stroke(); }
  const tex = new THREE.CanvasTexture(c); tex.colorSpace = THREE.SRGBColorSpace;
  const floor = new THREE.Mesh(new THREE.CircleGeometry(12, 64), new THREE.MeshStandardMaterial({ map: tex, roughness: .9 }));
  floor.rotation.x = -Math.PI / 2; floor.receiveShadow = true; scene.add(floor);
}

// ------------------------------------------------------------------ models
const MODELS = [
  { id: 'priya', name: 'Priya', file: 'models/priya.glb', height: 1.66, mat: 0xd0587a },
  { id: 'arjun', name: 'Arjun', file: 'models/arjun.glb', height: 1.78, mat: 0x3f7fc4 },
  { id: 'riya', name: 'Riya', file: 'models/riya.glb', height: 1.62, mat: 0x3e9a6a },
];
const BONES = ['Hips', 'Spine', 'Spine1', 'Spine2', 'Neck', 'Head', 'LeftShoulder', 'LeftArm', 'LeftForeArm', 'LeftHand', 'RightShoulder', 'RightArm', 'RightForeArm', 'RightHand',
  'LeftUpLeg', 'LeftLeg', 'LeftFoot', 'LeftToeBase', 'RightUpLeg', 'RightLeg', 'RightFoot', 'RightToeBase'];
const POSE_BONES = BONES.filter(b => !/Hand$|ToeBase/.test(b));
const ALIGN = [ // bone, child, target dir (model space)
  ['Hips', 'Spine', [0, 1, 0]], ['Spine', 'Spine1', [0, 1, 0]], ['Spine1', 'Spine2', [0, 1, 0]], ['Spine2', 'Neck', [0, 1, 0]], ['Neck', 'Head', [0, 1, .08]],
  ['LeftShoulder', 'LeftArm', [1, 0, 0]], ['LeftArm', 'LeftForeArm', [1, 0, 0]], ['LeftForeArm', 'LeftHand', [1, 0, 0]],
  ['RightShoulder', 'RightArm', [-1, 0, 0]], ['RightArm', 'RightForeArm', [-1, 0, 0]], ['RightForeArm', 'RightHand', [-1, 0, 0]],
  ['LeftUpLeg', 'LeftLeg', [0, -1, 0]], ['LeftLeg', 'LeftFoot', [0, -1, 0]], ['LeftFoot', 'LeftToeBase', [0, -.55, 1]],
  ['RightUpLeg', 'RightLeg', [0, -1, 0]], ['RightLeg', 'RightFoot', [0, -1, 0]], ['RightFoot', 'RightToeBase', [0, -.55, 1]],
];
const models = [];
const _v = new THREE.Vector3(), _v2 = new THREE.Vector3(), _q = new THREE.Quaternion(), _q2 = new THREE.Quaternion(), _e = new THREE.Euler();

const manager = new THREE.LoadingManager();
const loader = new GLTFLoader(manager).setMeshoptDecoder(MeshoptDecoder);

function applyClipPose(root, clip) {
  for (const tr of clip.tracks) {
    const [node, prop] = tr.name.split('.');
    if (prop !== 'quaternion') continue;
    const o = root.getObjectByName(node); if (!o) continue;
    o.quaternion.fromArray(tr.values, 0);
  }
}
function prepareModel(spec, gltf) {
  const container = new THREE.Group(), lift = new THREE.Group(), orient = new THREE.Group();
  container.add(lift); lift.add(orient); orient.add(gltf.scene); scene.add(container);
  const tp = gltf.animations.find(a => /tpose/i.test(a.name)); if (tp) applyClipPose(gltf.scene, tp);
  const bones = {}; const morphs = [];
  gltf.scene.traverse(o => {
    if (o.isBone) { const k = o.name.replace(/^mixamorig[:_]?/, ''); if (BONES.includes(k) && !bones[k]) bones[k] = o; }
    if (o.isMesh) {
      o.castShadow = true; o.receiveShadow = true; o.frustumCulled = false;
      if (o.material) { const ms = Array.isArray(o.material) ? o.material : [o.material]; ms.forEach(m => { if (m.map) m.map.anisotropy = 4; }); }
      if (o.morphTargetDictionary && Object.keys(o.morphTargetDictionary).some(k => /^(jawOpen|mouthOpen|tongueOut|eyeBlink|eyesClosed|mouthSmile)/.test(k))) morphs.push(o);
    }
  });
  // facing: forward = left x up
  scene.updateMatrixWorld(true);
  const lp = bones.LeftUpLeg.getWorldPosition(new THREE.Vector3()), rp = bones.RightUpLeg.getWorldPosition(new THREE.Vector3());
  const left = lp.sub(rp); left.y = 0; left.normalize();
  const fwd = new THREE.Vector3().crossVectors(left, new THREE.Vector3(0, 1, 0));
  orient.rotation.y = -Math.atan2(fwd.x, fwd.z);
  scene.updateMatrixWorld(true);
  // canonical T-pose alignment
  for (const [b, c, t] of ALIGN) {
    const B = bones[b], C = bones[c]; if (!B || !C) continue;
    scene.updateMatrixWorld(true);
    const pb = B.getWorldPosition(new THREE.Vector3()), pc = C.getWorldPosition(new THREE.Vector3());
    const d = pc.sub(pb).normalize(), target = new THREE.Vector3(...t).normalize();
    const rot = new THREE.Quaternion().setFromUnitVectors(d, target);
    const wq = B.getWorldQuaternion(new THREE.Quaternion());
    const pq = B.parent.getWorldQuaternion(new THREE.Quaternion());
    B.quaternion.copy(pq.invert().multiply(rot.multiply(wq)));
  }
  scene.updateMatrixWorld(true);
  // scale to height
  const meshes = []; gltf.scene.traverse(o => { if (o.isSkinnedMesh) meshes.push(o); });
  const box = new THREE.Box3();
  for (const m of meshes) { const n = m.geometry.attributes.position.count; for (let i = 0; i < n; i += 7) { m.getVertexPosition(i, _v); _v.applyMatrix4(m.matrixWorld); box.expandByPoint(_v); } }
  const h = box.max.y - box.min.y; orient.scale.setScalar(spec.height / h);
  scene.updateMatrixWorld(true);
  // rest data (relative to lift frame == model space)
  const rest = {};
  for (const k of BONES) {
    const B = bones[k]; if (!B) continue;
    const rel = B.getWorldQuaternion(new THREE.Quaternion());
    rest[k] = { local: B.quaternion.clone(), rel, A: B.quaternion.clone().multiply(rel.clone().invert()) };
  }
  // grounding samples
  const samples = [];
  const gm = meshes.filter(m => !m.morphTargetInfluences || !m.morphTargetInfluences.length); const tot = gm.reduce((a, m) => a + m.geometry.attributes.position.count, 0);
  for (const m of gm) { const n = m.geometry.attributes.position.count; const step = Math.max(1, Math.floor(tot / 380)); for (let i = 0; i < n; i += step) samples.push([m, i]); }
  // head height for camera
  const headY = bones.Head.getWorldPosition(new THREE.Vector3()).y;
  const md = {
    spec, container, lift, orient, root: gltf.scene, bones, rest, samples, morphs, headY,
    groundY: 0, blinkOff: Math.random() * 3, cur: pose({}), anim: null, home: new THREE.Vector3(), heading: 0, props: {}, visible: true,
  };
  md.lift.position.y = 0;
  buildProps(md);
  // label + mat
  const mat = new THREE.Mesh(new THREE.BoxGeometry(.9, .02, 2.0), new THREE.MeshStandardMaterial({ color: spec.mat, roughness: .85 }));
  mat.position.y = .01; mat.receiveShadow = true; md.mat = mat; scene.add(mat);
  return md;
}
function buildProps(md) {
  const stool = new THREE.Group();
  const wood = new THREE.MeshStandardMaterial({ color: 0x8b5a2b, roughness: .55 });
  const seatH = md.bones.LeftLeg.getWorldPosition(new THREE.Vector3()).y - .03;
  const seat = new THREE.Mesh(new THREE.CylinderGeometry(.24, .24, .05, 32), new THREE.MeshStandardMaterial({ color: 0x333a44, roughness: .5 }));
  seat.position.y = seatH; seat.castShadow = true; stool.add(seat);
  for (let i = 0; i < 4; i++) { const a = i * Math.PI / 2 + Math.PI / 4; const leg = new THREE.Mesh(new THREE.CylinderGeometry(.02, .025, seatH, 10), wood); leg.position.set(Math.cos(a) * .16, seatH / 2, Math.sin(a) * .16); leg.castShadow = true; stool.add(leg); }
  stool.position.z = -.12; stool.visible = false; md.container.add(stool); md.props.stool = stool;
  const dbs = [];
  for (const side of ['LeftHand', 'RightHand']) {
    const db = new THREE.Group(); const metal = new THREE.MeshStandardMaterial({ color: 0x22252a, metalness: .6, roughness: .35 });
    const bar = new THREE.Mesh(new THREE.CylinderGeometry(.014, .014, .3, 10), new THREE.MeshStandardMaterial({ color: 0xaaaaaa, metalness: .9, roughness: .25 }));
    db.add(bar); for (const y of [-.12, .12]) { const w = new THREE.Mesh(new THREE.CylinderGeometry(.055, .055, .07, 20), metal); w.position.y = y; w.castShadow = true; db.add(w); }
    const hb = md.bones[side];
    db.scale.setScalar(1 / hb.getWorldScale(new THREE.Vector3()).x);
    db.position.set(0, .08 / hb.getWorldScale(new THREE.Vector3()).x, .02 / hb.getWorldScale(new THREE.Vector3()).x);
    db.rotation.z = Math.PI / 2; db.visible = false; hb.add(db); dbs.push(db);
  }
  md.props.dumbbell = { set visible(v) { dbs.forEach(d => d.visible = v); } };
}

// ------------------------------------------------------------------ face + oral inset
const inset = { mode: null, cam: new THREE.PerspectiveCamera(26, 1, .02, 30), target: { jawOpen: 0, tongueOut: 0 }, cur: { jawOpen: 0, tongueOut: 0 }, md: null };
const oral = buildOral();

const _hq = new THREE.Quaternion(), _hp = new THREE.Vector3(), _fd = new THREE.Vector3(), _up = new THREE.Vector3(), _cp = new THREE.Vector3();
function aimHeadCam(md, dt) {
  const H = md.bones.Head; H.getWorldQuaternion(_hq); _hq.multiply(_q2.copy(md.rest.Head.rel).invert());
  // include lift/container rotation
  H.getWorldPosition(_hp);
  _fd.set(0, 0, 1).applyQuaternion(_hq); _up.set(0, 1, 0).applyQuaternion(_hq);
  const sc = md.spec.height / 1.7;
  _hp.addScaledVector(_up, .075 * sc).addScaledVector(_fd, .06 * sc);
  _cp.copy(_hp).addScaledVector(_fd, .5 * sc).addScaledVector(_up, .04 * sc);
  inset.cam.position.copy(_cp); inset.cam.up.copy(_up); inset.cam.lookAt(_hp.x - _up.x * .03 * sc, _hp.y - _up.y * .03 * sc, _hp.z - _up.z * .03 * sc);
  inset.cam.aspect = 1; inset.cam.updateProjectionMatrix();
}
// ------------------------------------------------------------------ pose engine
function evalFrames(frames, t) { // t in seconds within cycle; returns pose
  let acc = 0;
  for (let i = 0; i < frames.length; i++) {
    const f = frames[i], d = f.d + (f.hold || 0);
    if (t <= acc + d || i === frames.length - 1) {
      const prev = frames[(i - 1 + frames.length) % frames.length].p;
      const k = Math.min(1, Math.max(0, (t - acc) / f.d));
      return lerpPose(prev, f.p, ease(k));
    }
    acc += d;
  }
  return frames[0].p;
}
const cycleLen = frames => frames.reduce((s, f) => s + f.d + (f.hold || 0), 0);
const ease = k => k < .5 ? 2 * k * k : 1 - Math.pow(-2 * k + 2, 2) / 2;
function lerpPose(a, b, k) {
  const r = { b: {}, lift: a.lift + (b.lift - a.lift) * k, mouth: a.mouth + (b.mouth - a.mouth) * k, tongue: a.tongue + (b.tongue - a.tongue) * k };
  for (const n of POSE_BONES) { const x = a.b[n] || [0, 0, 0], y = b.b[n] || [0, 0, 0]; r.b[n] = [x[0] + (y[0] - x[0]) * k, x[1] + (y[1] - x[1]) * k, x[2] + (y[2] - x[2]) * k]; }
  return r;
}
function poseDist(a, b) { let m = 0; for (const n of POSE_BONES) { const x = a.b[n], y = b.b[n]; if (!x || !y) continue; for (let i = 0; i < 3; i++) m = Math.max(m, Math.abs(x[i] - y[i])); } return m; }
function procPose(kind, t) {
  if (kind === 'walk' || kind === 'turn') {
    const ph = t * Math.PI * 2 * .95, s = Math.sin(ph), c = Math.cos(ph);
    const amp = kind === 'turn' ? .5 : 1;
    const kl = 6 + 52 * Math.pow(Math.max(0, c), 1.6) * amp, kr = 6 + 52 * Math.pow(Math.max(0, -c), 1.6) * amp;
    return pose({
      ll: { f: 24 * s * amp, k: kl, an: -10 * Math.max(0, c) * amp + 8 * Math.max(0, -s) }, rl: { f: -24 * s * amp, k: kr, an: -10 * Math.max(0, -c) * amp + 8 * Math.max(0, s) },
      la: { f: -20 * s * amp, a: 8, e: 18 - 8 * s }, ra: { f: 20 * s * amp, a: 8, e: 18 + 8 * s }, spine: [3, 5 * s, 0], hips: [0, -6 * s * amp, 0], neck: [-2, -4 * s, 0]
    });
  }
  if (kind === 'run') {
    const ph = t * Math.PI * 2 * 1.45, s = Math.sin(ph), c = Math.cos(ph);
    return pose({
      trunk: 10, ll: { f: 40 * s + 12, k: 25 + 70 * Math.max(0, c), an: 15 * Math.max(0, -s) }, rl: { f: -40 * s + 12, k: 25 + 70 * Math.max(0, -c), an: 15 * Math.max(0, s) },
      la: { f: -45 * s, a: 12, e: 85 }, ra: { f: 45 * s, a: 12, e: 85 }, spine: [4, 8 * s, 0], hips: [0, -8 * s, 0], neck: [-8, -6 * s, 0], lift: .05 * Math.abs(Math.sin(ph))
    });
  }
  return pose({});
}
function setBones(md, p, time) {
  const breath = Math.sin(time * 1.6) * 1.2;
  for (const n of POSE_BONES) {
    const R = md.rest[n]; const B = md.bones[n]; if (!R || !B) continue;
    const d = p.b[n] || [0, 0, 0];
    let x = d[0]; if (n === 'Spine2') x -= breath * .6; if (n === 'Neck') x += breath * .3;
    _e.set(x * D2R, d[1] * D2R, d[2] * D2R, 'XYZ'); _q.setFromEuler(_e);
    B.quaternion.copy(R.A).multiply(_q).multiply(R.rel);
  }
  const blink = ((time + md.blinkOff) % 3.7) < .13 ? 1 : 0;
  const jaw = Math.max(p.mouth, p.tongue * .6);
  for (const m of md.morphs) {
    const d = m.morphTargetDictionary, f = m.morphTargetInfluences;
    const set = (k, v) => { if (d[k] !== undefined) f[d[k]] = v; };
    if (d.jawOpen !== undefined) { set('jawOpen', jaw); set('mouthOpen', jaw * .3); } else set('mouthOpen', jaw);
    set('tongueOut', p.tongue);
    set('eyeBlinkLeft', blink); set('eyeBlinkRight', blink); set('eyesClosed', blink);
    const sm = .22 * (1 - Math.min(1, jaw * 2)); set('mouthSmileLeft', sm); set('mouthSmileRight', sm); set('mouthSmile', sm);
    set('browInnerUp', jaw * .35);
  }
}
function groundModel(md, extraLift, enabled) {
  md.lift.position.y = md.groundY; md.container.updateMatrixWorld(true);
  if (enabled) {
    let mn = 1e9; for (const [m, i] of md.samples) { m.getVertexPosition(i, _v); _v.applyMatrix4(m.matrixWorld); if (_v.y < mn) mn = _v.y; }
    md.groundY -= (mn - .004) * .6;
  }
  md.lift.position.y = md.groundY + extraLift;
}

// ------------------------------------------------------------------ animation control
const state = { target: 'all', speed: 1, paused: false, count: true, lang: 'en', listening: false, routine: null, view: 'auto' };
function startAnim(md, def, opts = {}) {
  const first = def.proc ? procPose(def.proc, 0) : def.frames[0].p;
  const dist = poseDist(md.cur, first);
  md.anim = { def, t: 0, trans: Math.min(2.4, Math.max(.5, dist / 75)), from: md.cur, rep: 0, cycle: def.frames ? cycleLen(def.frames) : 1, opts, lastCycle: 0 };
  for (const k in md.props) md.props[k].visible = !!(def.props && def.props.includes(k));
}
function targets(name) { return models.filter(m => m.visible && (name ? m.spec.id === name : (state.target === 'all' || m.spec.id === state.target))); }
function walkPath(md, idx, n, t, speed) {
  const R = n > 1 ? 1.8 : 1.3, w = speed / R;
  const a = t * w + idx * Math.PI * 2 / n;
  md.container.position.set(Math.sin(a) * R, 0, Math.cos(a) * R - (n > 1 ? .6 : .2));
  md.container.rotation.y = a + Math.PI / 2;
}
function update(dt, time) {
  const vis = models.filter(m => m.visible);
  vis.forEach((md, idx) => {
    const A = md.anim; let p = md.cur;
    if (A && !state.paused) {
      A.t += dt * state.speed;
      const def = A.def;
      let target;
      const tt = Math.max(0, A.t - A.trans);
      if (def.proc) target = procPose(def.proc, tt);
      else if (def.hold) target = def.frames[def.frames.length - 1].p;
      else {
        const cyc = A.cycle; const c = Math.floor(tt / cyc);
        if (c > A.lastCycle) { A.lastCycle = c; A.rep = c; onRep(md, A); }
        target = evalFrames(def.frames, tt % cyc);
      }
      p = A.t < A.trans ? lerpPose(A.from, target, ease(A.t / A.trans)) : target;
      md.cur = p;
      // locomotion
      if (def.move && A.t > A.trans * .5) { A.mt = (A.mt || 0) + dt * state.speed; walkPath(md, idx, vis.length, A.mt + (A.mt0 || 0), def.move); }
      else if (def.proc === 'turn') { md.heading += dt * state.speed * 1.3; if (md.heading >= A.opts.goal) { md.heading = A.opts.goal; if (A.t > A.trans) startAnim(md, BASIC.stand); } md.container.rotation.y = md.heading; }
      else { // return home smoothly
        md.container.position.lerp(md.home, Math.min(1, dt * 2.5));
        let dr = md.heading - md.container.rotation.y; dr = Math.atan2(Math.sin(dr), Math.cos(dr)); md.container.rotation.y += dr * Math.min(1, dt * 3);
      }
    }
    setBones(md, p, time);
    groundModel(md, p.lift || 0, true);
    md.mat.position.set(md.home.x, .01, md.home.z); md.mat.rotation.y = md.heading; md.mat.visible = md.visible && !(md.anim && md.anim.def.move);
  });
}

// ------------------------------------------------------------------ reps, routine, speech
function onRep(md, A) {
  if (md !== targets()[0]) return; // count once
  const def = A.def; const n = A.rep;
  setRepLabel(n, def.reps);
  if (state.count && n > 0) speak(numberWord(n));
  if (state.routine && def.reps && n >= def.reps) nextInRoutine();
}
function numberWord(n) { if (state.lang === 'hi') return ['', 'एक', 'दो', 'तीन', 'चार', 'पाँच', 'छह', 'सात', 'आठ', 'नौ', 'दस', 'ग्यारह', 'बारह', 'तेरह', 'चौदह', 'पंद्रह', 'सोलह', 'सत्रह', 'अठारह', 'उन्नीस', 'बीस'][n] || String(n); return String(n); }
function speak(text, guard = false) {
  if (NATIVE && NATIVE.speak) { try { NATIVE.speak(text, state.lang === 'hi' ? 'hi-IN' : 'en-IN', guard); } catch (e) { } return; }
  if (window.speechSynthesis) { const u = new SpeechSynthesisUtterance(text); u.lang = state.lang === 'hi' ? 'hi-IN' : 'en-IN'; speechSynthesis.cancel(); speechSynthesis.speak(u); }
}
function nextInRoutine() {
  const r = state.routine; r.i++;
  if (r.i >= r.list.length) { state.routine = null; runBasic('stand'); speak(state.lang === 'hi' ? 'व्यायाम पूरा हुआ। शाबाश!' : 'Routine complete. Well done!', true); return; }
  runExercise(r.list[r.i].id, r.name, true);
}

// ------------------------------------------------------------------ commands
function setCaption(title, steps, sub) {
  $('#capTitle').textContent = title;
  $('#capSteps').innerHTML = (steps || []).map((s, i) => `<li>${s}</li>`).join('');
  $('#capSub').textContent = sub || '';
  $('#caption').classList.toggle('show', !!title); $('#caption').classList.remove('mini');
  clearTimeout(setCaption.tm); setCaption.tm = setTimeout(() => $('#caption').classList.add('mini'), 7000);
}
function setRepLabel(n, total) { $('#reps').textContent = total ? `Rep ${Math.min(n, total)} / ${total}` : (n ? `Rep ${n}` : ''); }
function setInset(mode, face) {
  inset.mode = mode; $('#insetFrame').classList.toggle('show', !!mode); $('#oralCtl').style.display = mode === 'oral' ? 'flex' : 'none';
  $('#insetLabel').textContent = mode === 'oral' ? 'Oropharynx view – tongue depressed, "Aaah"' : mode === 'face' ? 'Close-up' : '';
  inset.target.jawOpen = face ? (face.jawOpen || 0) : 0; inset.target.tongueOut = face ? (face.tongueOut || 0) : 0;
  if (mode === 'face') { const vis = targets(); const hasT = vis.find(m => m.morphs.some(x => x.morphTargetDictionary.tongueOut !== undefined)); const hasJ = vis.find(m => m.morphs.some(x => x.morphTargetDictionary.jawOpen !== undefined)); inset.md = (face && face.tongueOut ? hasT : null) || hasJ || hasT || vis[0]; $('#insetLabel').textContent = 'Close-up: ' + (inset.md ? inset.md.spec.name : ''); }
}
function runBasic(id, who) {
  const def = BASIC[id]; if (!def) return;
  if (id !== 'stand' || !who) state.routine = state.routine && who === undefined ? state.routine : null;
  const ts = targets(who);
  ts.forEach(md => {
    if (id === 'turn') { const goal = md.heading + Math.PI; startAnim(md, def, { goal }); return; }
    if (def.move) { startAnim(md, def); }
    else startAnim(md, def);
  });
  if (def.inset !== undefined || ['closemouth', 'stand'].includes(id)) setInset(def.inset || null, def.face);
  else if (!['openmouth', 'tongue', 'tonsils'].includes(id)) setInset(null);
  const tip = { openmouth: ['Mouth wide open.'], tongue: ['Tongue protruded fully.'], tonsils: ['Head tilted back, mouth wide, say "Aaah".', 'Use grade buttons to compare tonsil sizes (Brodsky 0–4).'] }[id];
  setCaption(def.name, tip, who ? names(ts) : '');
  setRepLabel(0, 0);
  autoView(def);
  speak(def.name.replace(/–.*/, ''), true);
}
function runExercise(id, routineName, fromRoutine, who) {
  const e = EXMAP[id]; if (!e) return;
  if (!fromRoutine) state.routine = null;
  targets(who).forEach(md => startAnim(md, e));
  setInset(null);
  const sub = state.routine ? `${routineName} · ${state.routine.i + 1} of ${state.routine.list.length}` : '';
  setCaption(e.name, e.steps, sub);
  setRepLabel(0, e.reps);
  autoView(e);
  speak(e.name + '. ' + e.steps.join(' '), true);
  highlightChip(id);
}
function runCategory(cat) {
  const list = EXERCISES.filter(e => e.cats.includes(cat)); if (!list.length) return;
  const c = CATEGORIES.find(c => c.id === cat);
  state.routine = { list, i: 0, name: c.name + ' routine' };
  runExercise(list[0].id, state.routine.name, true);
}
function stopAll() { state.routine = null; runBasic('stand'); }
function names(ts) { return ts.map(m => m.spec.name).join(', '); }

// matching voice/text commands
function norm(s) { return s.toLowerCase().replace(/[.,!?।]/g, ' ').replace(/\s+/g, ' ').trim(); }
function matchCommand(text) {
  const t = ' ' + norm(text) + ' ';
  let best = null;
  const tryList = (dict, type) => { for (const id in dict) for (const ph of dict[id]) { const p = norm(ph); const ok = /[a-z]/.test(p) ? t.includes(' ' + p + ' ') || t.includes(' ' + p) && p.length > 5 : t.includes(p); if (ok && (!best || p.length > best.len)) best = { type, id, len: p.length + (type === 'ex' ? .5 : 0) }; } };
  tryList(PHRASES, 'basic'); tryList(EX_PHRASES, 'ex'); tryList(CAT_PHRASES, 'cat');
  let who; const toks = t.trim().split(' '); for (const m of models) if (toks.includes(m.spec.name.toLowerCase()) || toks.includes({ priya: 'प्रिया', arjun: 'अर्जुन', riya: 'रिया' }[m.spec.id])) who = m.spec.id;
  if (/\b(all|everyone|everybody|sab|sabhi)\b|सब|सभी/.test(t)) who = undefined;
  return best ? Object.assign(best, { who }) : null;
}
function execText(text) {
  $('#heard').textContent = '“' + text + '”'; $('#heard').classList.add('show'); clearTimeout(execText.tm); execText.tm = setTimeout(() => $('#heard').classList.remove('show'), 3500);
  const m = matchCommand(text);
  if (!m) { $('#heard').textContent += '  – not understood'; return false; }
  if (m.who && !models.find(x => x.spec.id === m.who).visible) selectTarget('all');
  if (m.type === 'basic') { if (m.id === 'stop') stopAll(); else runBasic(m.id, m.who); }
  else if (m.type === 'ex') runExercise(m.id, null, false, m.who);
  else runCategory(m.id);
  return true;
}
window.BodyCoach = { execText };

// ------------------------------------------------------------------ voice input
let webRec = null;
function startListening() {
  state.listening = true; $('#mic').classList.add('on');
  const lang = state.lang === 'hi' ? 'hi-IN' : 'en-IN';
  if (NATIVE && NATIVE.startListening) { NATIVE.startListening(lang); return; }
  const SR = window.SpeechRecognition || window.webkitSpeechRecognition;
  if (!SR) { toast('Voice recognition not available here – type the command instead.'); stopListening(); return; }
  webRec = new SR(); webRec.lang = lang; webRec.interimResults = true; webRec.continuous = true;
  webRec.onresult = e => { const r = e.results[e.results.length - 1]; window.onVoiceResult(r[0].transcript, r.isFinal); };
  webRec.onend = () => { if (state.listening) try { webRec.start(); } catch (e) { } };
  webRec.start();
}
function stopListening() { state.listening = false; $('#mic').classList.remove('on'); if (NATIVE && NATIVE.stopListening) NATIVE.stopListening(); if (webRec) { webRec.onend = null; webRec.stop(); webRec = null; } }
window.onVoiceResult = (text, final) => {
  if (!final) { $('#heard').textContent = '… ' + text; $('#heard').classList.add('show'); return; }
  execText(text);
};
window.onVoiceFinal = (arr) => {
  for (const t of arr) if (matchCommand(t)) { execText(t); return; }
  execText(arr[0]);
};
window.onVoiceState = () => { };
window.onVoiceError = (msg) => {
  if (msg === 'permission') { toast('Microphone permission is needed for voice commands.'); stopListening(); }
  else if (msg === 'unavailable') { toast('No speech recogniser on this phone. Install/enable "Speech Recognition & Synthesis by Google". You can still tap or type commands.'); stopListening(); }
  else if (msg === 'offline') { toast(state.lang === 'hi' ? 'ऑफ़लाइन हिंदी वॉइस पैक डाउनलोड करें (Help देखें)' : 'Offline speech pack needed for voice without internet – see Help (?)'); }
};
function toast(t) { const el = $('#toast'); el.textContent = t; el.classList.add('show'); clearTimeout(toast.tm); toast.tm = setTimeout(() => el.classList.remove('show'), 3000); }

// ------------------------------------------------------------------ camera views & layout
const camGoal = { pos: new THREE.Vector3(0, 1.45, 6.2), tgt: new THREE.Vector3(0, .95, 0), active: true };
function layoutFor(def) {
  if (!def) return 'row';
  if (def.view === 'high' || def.view === 'side') return 'class';
  if (def.cam === 'side') return 'profile';
  return 'row';
}
function autoView(def) {
  const lay = layoutFor(def); layoutModels(lay);
  applyView(state.view === 'auto' ? lay : state.view, def && def.cam);
}
function applyView(v, focus) {
  userCam = false;
  const n = models.filter(m => m.visible).length, W = n === 1 ? .8 : 1;
  const map = {
    row: [[0, 1.3, 6.0 * W], [0, .9, 0]],
    profile: [[0, 1.3, 6.0 * W], [0, .9, 0]],
    class: [[.3 * W, 2.4 * W + .3, 5.4 * W + .5], [0, .3, n === 1 ? 0 : .3]],
    front: [[0, 1.3, 6.0 * W], [0, .9, 0]],
    side3: [[4.0 * W, 1.8, 4.2 * W], [0, .7, 0]],
    high: [[3.2 * W, 3.6 * W, 3.8 * W], [0, .3, 0]],
    top: [[0, 7 * W, .9], [0, 0, 0]],
  };
  let [p, t] = map[v] || map.row;
  if (v === 'row' || v === 'profile' || v === 'front') {
    if (focus === 'head') { p = [0, 1.25, 3.6 * W]; t = [0, 1.0, 0]; }
    if (focus === 'upper') { p = [0, 1.45, 4.2 * W]; t = [0, 1.15, 0]; }
    if (focus === 'legs') { p = [0, 1.0, 4.4 * W]; t = [0, .55, 0]; }
  }
  if (focus === 'feet' && v === 'class') { p = [p[0], p[1] * .8, p[2] * .8]; }
  const zk = state.zk || 1; camGoal.tgt.set(...t); camGoal.pos.set(...p).sub(camGoal.tgt).multiplyScalar(zk).add(camGoal.tgt); camGoal.active = true;
}
function layoutModels(mode) {
  state.layout = mode || state.layout || 'row';
  const vis = models.filter(m => m.visible); const n = vis.length;
  vis.forEach((md, i) => {
    const k = i - (n - 1) / 2;
    if (state.layout === 'class') { md.home.set(k * .35, 0, k * 1.45); md.heading = -Math.PI / 2; }
    else if (state.layout === 'profile') { md.home.set(k * 1.7, 0, 0); md.heading = -Math.PI / 2; }
    else { md.home.set(k * 1.2, 0, 0); md.heading = 0; }
  });
  models.forEach(md => { md.container.visible = md.visible; md.mat.visible = md.visible; });
}
function selectTarget(id) {
  state.target = id;
  models.forEach(m => m.visible = id === 'all' || m.spec.id === id);
  models.forEach(md => { md.container.position.copy(md.home); });
  document.querySelectorAll('#who button').forEach(b => b.classList.toggle('on', b.dataset.id === id));
  const cur = models.find(m => m.visible && m.anim); autoView(cur ? cur.anim.def : null);
  models.filter(m => m.visible).forEach(md => { md.container.position.copy(md.home); md.container.rotation.y = md.heading; });
}

// ------------------------------------------------------------------ UI
function buildUI() {
  const who = $('#who');
  [['all', 'All'], ...MODELS.map(m => [m.id, m.name])].forEach(([id, n]) => { const b = document.createElement('button'); b.textContent = n; b.dataset.id = id; b.onclick = () => selectTarget(id); who.appendChild(b); });
  const tabs = $('#tabs');
  CATEGORIES.forEach((c, i) => { const b = document.createElement('button'); b.dataset.cat = c.id; b.innerHTML = `${c.name}<small>${c.hi}</small>`; b.onclick = () => showCat(c.id); tabs.appendChild(b); });
  showCat('basic');
  $('#mic').onclick = () => state.listening ? stopListening() : startListening();
  $('#lang').onclick = () => { state.lang = state.lang === 'en' ? 'hi' : 'en'; $('#lang').textContent = state.lang === 'en' ? 'EN' : 'हिं'; if (state.listening) { stopListening(); startListening(); } toast(state.lang === 'hi' ? 'हिंदी में बोलें: "बैठो", "चलो", "मुंह खोलो"…' : 'Speak in English: "sit", "walk", "open mouth"…'); };
  $('#cmdForm').onsubmit = e => { e.preventDefault(); const v = $('#cmd').value.trim(); if (v) { execText(v); $('#cmd').value = ''; $('#cmd').blur(); } };
  $('#pause').onclick = () => { state.paused = !state.paused; $('#pause').textContent = state.paused ? '▶' : '❚❚'; };
  $('#stop').onclick = stopAll;
  $('#speed').oninput = e => { state.speed = +e.target.value; $('#speedV').textContent = state.speed.toFixed(1) + '×'; };
  $('#count').onclick = () => { state.count = !state.count; $('#count').classList.toggle('on', state.count); };
  $('#view').onclick = () => { const order = ['auto', 'front', 'side3', 'high', 'top']; state.view = order[(order.indexOf(state.view) + 1) % order.length]; $('#view').textContent = { auto: 'View: Auto', front: 'View: Front', side3: 'View: Side', high: 'View: Above', top: 'View: Top' }[state.view]; const md = models.find(m => m.anim); autoView(md && md.anim ? md.anim.def : {}); };
  $('#caption').onclick = () => { $('#caption').classList.toggle('mini'); };
  $('#help').onclick = () => $('#helpBox').classList.toggle('show');
  $('#helpBox').onclick = () => $('#helpBox').classList.remove('show');
  $('#toggleDrawer').onclick = () => { document.body.classList.toggle('collapsed'); setTimeout(resize, 320); };
  document.querySelectorAll('#oralCtl button').forEach(b => b.onclick = () => {
    const g = b.dataset.g; if (g === 'red') { oral.state.red = !oral.state.red; b.classList.toggle('on', oral.state.red); return; }
    oral.setGrade(+g); document.querySelectorAll('#oralCtl button[data-g]:not([data-g=red])').forEach(x => x.classList.toggle('on', x === b));
  });
  $('#labelsBtn').onclick = () => { state.labels = !state.labels; $('#labelsBtn').classList.toggle('on', state.labels); };
  state.labels = true;
}
function showCat(cat) {
  document.querySelectorAll('#tabs button').forEach(b => b.classList.toggle('on', b.dataset.cat === cat));
  const box = $('#chips'); box.innerHTML = '';
  const add = (label, fn, cls, id) => { const b = document.createElement('button'); b.textContent = label; if (cls) b.className = cls; if (id) b.dataset.id = id; b.onclick = fn; box.appendChild(b); };
  if (cat === 'basic') BASIC_ORDER.forEach(id => add(BASIC[id].name, () => runBasic(id), '', id));
  else if (cat === 'oral') ORAL_ORDER.forEach(id => add(BASIC[id].name.replace(/ –.*/, ''), () => runBasic(id), '', id));
  else {
    add('▶ Full routine', () => runCategory(cat), 'routine');
    EXERCISES.filter(e => e.cats.includes(cat)).forEach(e => add(e.name, () => runExercise(e.id), '', e.id));
  }
}
function highlightChip(id) { document.querySelectorAll('#chips button').forEach(b => b.classList.toggle('cur', b.dataset.id === id)); }

// ------------------------------------------------------------------ render loop
const clock = new THREE.Timer(); let time = 0;
function resize() {
  const w = innerWidth, h = innerHeight; renderer.setSize(w, h, false); camera.aspect = w / h;
  const dh = document.body.classList.contains('collapsed') ? 30 : Math.min(h * .45, $('#drawer').offsetHeight * .85);
  camera.setViewOffset(w, h + dh, 0, dh, w, h); camera.updateProjectionMatrix(); state.zk = (h + dh) / h * (w < h ? 1.35 : 1);
  if (models.length) { const cur = models.find(m => m.visible && m.anim); applyView(state.view === 'auto' ? layoutFor(cur && cur.anim.def) : state.view, cur && cur.anim.def.cam); }
  const s = Math.round(Math.min(w, h) * (w > h ? .46 : .5)); inset.size = s;
  const fr = $('#insetFrame'); fr.style.width = fr.style.height = s + 'px';
}
addEventListener('resize', resize);
function insetRect() { const fr = $('#insetFrame').getBoundingClientRect(); return { x: fr.left, y: innerHeight - fr.bottom, w: fr.width, h: fr.height }; }
function loop() {
  requestAnimationFrame(loop);
  clock.update(); window.__fc = (window.__fc || 0) + 1; const dt = Math.min(.05, clock.getDelta()); time += dt;
  if (models.length) update(dt, time);
  if (camGoal.active && !userCam) {
    camera.position.lerp(camGoal.pos, Math.min(1, dt * 2.2)); controls.target.lerp(camGoal.tgt, Math.min(1, dt * 2.2));
    if (camera.position.distanceTo(camGoal.pos) < .02) camGoal.active = false;
  }
  controls.update();
  renderer.setViewport(0, 0, innerWidth, innerHeight); renderer.setScissorTest(false); renderer.clear();
  renderer.render(scene, camera);
  if (inset.mode) {
    const r = insetRect(); renderer.setScissorTest(true); renderer.setScissor(r.x, r.y, r.w, r.h); renderer.setViewport(r.x, r.y, r.w, r.h); renderer.clear();
    if (inset.mode === 'face' && inset.md) {
      aimHeadCam(inset.md, dt); renderer.render(scene, inset.cam);
    } else if (inset.mode === 'oral') {
      oral.update(time); oral.cam.aspect = 1; oral.cam.updateProjectionMatrix(); renderer.render(oral.scene, oral.cam);
    }
    renderer.setScissorTest(false);
  }
  drawLabels();
}
function drawLabels() {
  const box = $('#oralLabels');
  if (inset.mode !== 'oral' || !state.labels) { if (box.childElementCount) box.innerHTML = ''; return; }
  if (!box.childElementCount) oral.labels.forEach(l => { const d = document.createElement('span'); d.textContent = l.t; box.appendChild(d); });
  const s = $('#insetFrame').clientWidth;
  oral.labels.forEach((l, i) => { _v.copy(l.p).project(oral.cam); const el = box.children[i]; el.style.left = ((_v.x * .5 + .5) * s) + 'px'; el.style.top = ((-_v.y * .5 + .5) * s) + 'px'; });
}

// ------------------------------------------------------------------ boot
async function boot() {
  buildUI(); resize(); loop();
  const prog = $('#loadTxt');
  manager.onProgress = (u, a, b) => { prog.textContent = `Loading models… ${Math.round(a / b * 100)}%`; };
  const gl = await Promise.all(MODELS.map(m => loader.loadAsync(m.file)));
  gl.forEach((g, i) => models.push(prepareModel(MODELS[i], g)));
  selectTarget('all');
  models.forEach(md => { md.cur = pose({}); startAnim(md, BASIC.stand); md.anim.t = 5; });
  $('#loading').classList.add('done');
  setCaption('Namaste! Tap the mic and give a command', ['Try: "sit", "walk", "jump", "open your mouth", "show tonsils", "lie on your back"…', 'Or: "exercise for backache", "sciatica", "Priya do push ups".'], '');
  window.__ready = true;
}
boot();
// test hooks
window.__test = { fps: () => window.__fps, models, runBasic, runExercise, BASIC, EXMAP, state, applyView, selectTarget, setTime: s => { document.querySelector("#caption").classList.add("mini"); models.forEach(m => { if (m.anim) m.anim.t = s + m.anim.trans; m.groundY = m.groundY; }); camera.position.copy(camGoal.pos); controls.target.copy(camGoal.tgt); inset.cur.jawOpen = inset.target.jawOpen; inset.cur.tongueOut = inset.target.tongueOut; } };
