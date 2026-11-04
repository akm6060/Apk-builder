// Procedural 3D oropharynx: view through a wide-open mouth with the tongue depressed ("Aaah")
import * as THREE from 'three';

function bumpy(geo, amp, freq, seed = 1) {
  const p = geo.attributes.position, v = new THREE.Vector3(), n = new THREE.Vector3();
  geo.computeVertexNormals(); const nr = geo.attributes.normal;
  for (let i = 0; i < p.count; i++) {
    v.fromBufferAttribute(p, i); n.fromBufferAttribute(nr, i);
    const k = Math.sin(v.x * freq + seed) * Math.sin(v.y * freq * 1.3 + seed * 2) * Math.sin(v.z * freq * .9 + seed * 3)
      + .5 * Math.sin(v.x * freq * 2.7 + seed) * Math.sin(v.y * freq * 2.1) * Math.sin(v.z * freq * 3.1);
    v.addScaledVector(n, k * amp); p.setXYZ(i, v.x, v.y, v.z);
  }
  geo.computeVertexNormals(); return geo;
}
const wet = (color, rough = .35, extra = {}) => new THREE.MeshPhysicalMaterial(Object.assign({ color, roughness: rough, clearcoat: .8, clearcoatRoughness: .2 }, extra));
function archPath(w, top, bottom, shoulder) {
  const s = new THREE.Path();
  s.moveTo(-w, bottom); s.lineTo(-w, shoulder);
  s.bezierCurveTo(-w, shoulder + (top - shoulder) * .75, -w * .35, top, 0, top);
  s.bezierCurveTo(w * .35, top, w, shoulder + (top - shoulder) * .75, w, shoulder);
  s.lineTo(w, bottom); s.lineTo(-w, bottom);
  return s;
}
function curtain(w, top, bottom, shoulder, color, z) {
  const sh = new THREE.Shape(); sh.moveTo(-4, -4); sh.lineTo(4, -4); sh.lineTo(4, 4); sh.lineTo(-4, 4); sh.lineTo(-4, -4);
  sh.holes.push(archPath(w, top, bottom, shoulder));
  const m = new THREE.Mesh(new THREE.ShapeGeometry(sh, 48), wet(color, .4, { side: THREE.DoubleSide })); m.position.z = z;
  const pts = archPath(w, top, bottom, shoulder).getSpacedPoints(80).slice(0, -1).filter(p => p.y > bottom + .01).map(p => new THREE.Vector3(p.x, p.y, 0));
  m.add(new THREE.Mesh(new THREE.TubeGeometry(new THREE.CatmullRomCurve3(pts), 120, .09, 12), wet(color, .32)));
  return m;
}

export function buildOral() {
  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0x140506);
  const cam = new THREE.PerspectiveCamera(40, 1, .05, 50);
  cam.position.set(0, .1, 7.3); cam.lookAt(0, -.1, 0);
  const torch = new THREE.SpotLight(0xfff6ea, 90, 16, .5, .45, 1.2);
  torch.position.set(.25, .35, 8.5); torch.target.position.set(0, -.1, -2); scene.add(torch, torch.target);
  scene.add(new THREE.AmbientLight(0xff8f8f, .25));
  scene.add(new THREE.HemisphereLight(0xffd8d0, 0x401010, .35));

  const g = new THREE.Group(); scene.add(g);
  const cav = new THREE.Mesh(new THREE.SphereGeometry(1, 48, 32), wet(0xd27a78, .45, { side: THREE.BackSide }));
  cav.scale.set(2.0, 1.55, 2.6); cav.position.set(0, -.1, .2); g.add(cav);
  const wall = new THREE.Mesh(bumpy(new THREE.PlaneGeometry(4, 4, 80, 80), .025, 9), wet(0x9c3236, .7, { clearcoat: .4 }));
  wall.position.z = -2.7; g.add(wall);
  const gran = wet(0xc95c5d, .3);
  for (let i = 0; i < 22; i++) { const s = new THREE.Mesh(new THREE.SphereGeometry(.05 + Math.random() * .045, 10, 8), gran); s.position.set((Math.random() - .5) * 1.3, -.6 + Math.random() * 1.3, -2.68); s.scale.z = .5; g.add(s); }
  const lumen = new THREE.Mesh(new THREE.CircleGeometry(.55, 40), new THREE.MeshBasicMaterial({ color: 0x1c0303 })); lumen.position.set(0, -1.05, -2.65); lumen.scale.y = .45; g.add(lumen);
  g.add(curtain(.82, .78, -1.6, .05, 0xb24a4e, -1.95));
  g.add(curtain(1.08, 1.0, -1.6, .15, 0xcf6a6e, -1.15));
  const uvula = new THREE.Group(); uvula.position.set(0, 1.02, -1.2); g.add(uvula);
  const uvMat = wet(0xd45f64, .28);
  const uv = new THREE.Mesh(new THREE.CapsuleGeometry(.12, .42, 8, 20), uvMat); uv.position.y = -.3; uvula.add(uv);
  const uvTip = new THREE.Mesh(new THREE.SphereGeometry(.15, 24, 16), uvMat); uvTip.position.y = -.55; uvula.add(uvTip);
  const tonsils = [];
  for (const s of [-1, 1]) {
    const geo = bumpy(new THREE.SphereGeometry(.42, 64, 48), .03, 11, s * 3);
    const col = []; const p = geo.attributes.position, v = new THREE.Vector3();
    for (let i = 0; i < p.count; i++) { v.fromBufferAttribute(p, i); const n = Math.sin(v.x * 26) * Math.sin(v.y * 21 + 1) * Math.sin(v.z * 23 + 2); const d = n > .6 ? .45 : 1; col.push(d, d * .96, d * .96); }
    geo.setAttribute('color', new THREE.Float32BufferAttribute(col, 3));
    const m = new THREE.Mesh(geo, wet(0xe0848a, .5, { vertexColors: true }));
    m.position.set(s * 1.0, -.45, -1.55); m.scale.set(.6, 1.0, .55); g.add(m); tonsils.push(m);
  }
  const tongue = new THREE.Mesh(bumpy(new THREE.SphereGeometry(1, 64, 40), .012, 18), wet(0xd88482, .55));
  tongue.scale.set(1.75, .62, 1.85); tongue.position.set(0, -1.75, .35); g.add(tongue);
  const groove = new THREE.Mesh(new THREE.CapsuleGeometry(.025, 1.6, 4, 8), new THREE.MeshStandardMaterial({ color: 0xa65252, roughness: .6 }));
  groove.rotation.x = Math.PI / 2 - .12; groove.position.set(0, -1.14, .9); g.add(groove);
  const dep = new THREE.Mesh(new THREE.BoxGeometry(.5, .04, 3.6), new THREE.MeshStandardMaterial({ color: 0xe6cc9e, roughness: .85 }));
  dep.position.set(.05, -1.1, 2.2); dep.rotation.x = -.1; g.add(dep);
  const toothMat = new THREE.MeshPhysicalMaterial({ color: 0xf7f2e8, roughness: .22, clearcoat: .7 });
  const toothGeo = new THREE.CapsuleGeometry(.11, .16, 6, 12);
  for (const [y, sgn] of [[1.12, 1], [-1.32, -1]]) {
    for (let i = -6; i <= 6; i++) {
      const a = i * .15, R = 1.75; const t = new THREE.Mesh(toothGeo, toothMat);
      t.position.set(Math.sin(a) * R, y - sgn * Math.abs(i) * .012, 1.75 - (1 - Math.cos(a)) * R * 1.4);
      t.scale.set(1 + (Math.abs(i) > 3 ? .25 : 0), 1, .8); t.rotation.y = a; g.add(t);
    }
  }
  const lip = new THREE.Mesh(new THREE.TorusGeometry(1.95, .34, 28, 80), new THREE.MeshPhysicalMaterial({ color: 0xb46f68, roughness: .42, clearcoat: .35 }));
  lip.scale.set(1.1, .9, .7); lip.position.set(0, -.12, 2.35); g.add(lip);
  const face = new THREE.Mesh(new THREE.RingGeometry(2.1, 7, 72), new THREE.MeshStandardMaterial({ color: 0xc8937a, roughness: .7 }));
  face.scale.set(1.1, .9, 1); face.position.set(0, -.12, 2.42); g.add(face);

  const labels = [
    { t: 'Uvula', p: new THREE.Vector3(0, .35, -1.0) },
    { t: 'Tonsil (L)', p: new THREE.Vector3(.98, -.85, -1.2) },
    { t: 'Tonsil (R)', p: new THREE.Vector3(-.98, -.85, -1.2) },
    { t: 'Anterior pillar', p: new THREE.Vector3(1.2, .55, -1.1) },
    { t: 'Soft palate', p: new THREE.Vector3(0, 1.2, -1.1) },
    { t: 'Posterior wall', p: new THREE.Vector3(.35, -.15, -2.6) },
    { t: 'Tongue', p: new THREE.Vector3(-.85, -1.1, 1.3) },
  ];
  const state = { grade: 1, red: false };
  const medial = [.0, .12, .3, .5, .72]; // Brodsky 0–4
  function setGrade(gr) { state.grade = gr; }
  function update(t) {
    const m = medial[state.grade];
    tonsils.forEach((ton, i) => {
      const s = i ? 1 : -1;
      const tx = s * (1.02 - m * .95); ton.position.x += (tx - ton.position.x) * .15;
      const sc = .45 + m * .55; ton.scale.x += (sc - ton.scale.x) * .15; ton.scale.y += ((.85 + m * .3) - ton.scale.y) * .15;
      ton.material.color.set(state.red ? 0xe0383a : 0xe0848a); ton.material.emissive.set(state.red ? 0x400000 : 0x000000);
    });
    const aah = .5 + .5 * Math.sin(t * 1.2);
    uvula.rotation.z = Math.sin(t * 2.0) * .05;
    uvula.position.y = 1.02 + aah * .08; uvula.scale.y = 1 - aah * .12;
  }
  return { scene, cam, update, setGrade, state, labels };
}
