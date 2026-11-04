// Procedural 3D oropharynx (view through an open mouth with tongue depressed)
import * as THREE from 'three';

function noiseDisplace(geo, amp, freq, seed = 1) {
  const p = geo.attributes.position, v = new THREE.Vector3();
  for (let i = 0; i < p.count; i++) {
    v.fromBufferAttribute(p, i);
    const n = Math.sin(v.x * freq + seed) * Math.sin(v.y * freq * 1.3 + seed * 2) * Math.sin(v.z * freq * .9 + seed * 3)
      + .5 * Math.sin(v.x * freq * 2.7 + seed) * Math.sin(v.y * freq * 2.1) * Math.sin(v.z * freq * 3.1);
    v.addScaledVector(v.clone().normalize(), n * amp);
    p.setXYZ(i, v.x, v.y, v.z);
  }
  geo.computeVertexNormals();
  return geo;
}
const wet = (color, rough = .38, extra = {}) => new THREE.MeshPhysicalMaterial(Object.assign({ color, roughness: rough, clearcoat: .7, clearcoatRoughness: .25, sheen: .3, sheenColor: new THREE.Color(0xffb0b0) }, extra));

export function buildOral() {
  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0x120405);
  const cam = new THREE.PerspectiveCamera(42, 1, .05, 50);
  cam.position.set(0, .15, 5.2); cam.lookAt(0, .05, 0);
  const torch = new THREE.SpotLight(0xfff4e6, 60, 14, .55, .5, 1.4);
  torch.position.set(.35, .5, 6); torch.target.position.set(0, 0, -1); scene.add(torch, torch.target);
  scene.add(new THREE.AmbientLight(0xff9a9a, .35));
  const fill = new THREE.PointLight(0xffd0c0, 6, 6); fill.position.set(0, .2, 1.5); scene.add(fill);

  const g = new THREE.Group(); scene.add(g);
  // posterior pharyngeal wall (granular)
  const wallGeo = noiseDisplace(new THREE.SphereGeometry(2.6, 64, 48, 0, Math.PI * 2, 0, Math.PI / 2), .04, 7);
  const wall = new THREE.Mesh(wallGeo, wet(0xb8474a, .45)); wall.rotation.x = -Math.PI / 2; wall.position.z = -1.4; wall.scale.set(.75, .9, .7); g.add(wall);
  const granMat = wet(0xc95a5a, .35);
  for (let i = 0; i < 26; i++) { const s = new THREE.Mesh(new THREE.SphereGeometry(.05 + Math.random() * .05, 10, 8), granMat); s.position.set((Math.random() - .5) * 1.6, (Math.random() - .3) * 1.4, -2.95 + Math.random() * .05); s.scale.z = .5; g.add(s); }
  // dark oropharyngeal lumen hint
  const lumen = new THREE.Mesh(new THREE.CircleGeometry(.55, 32), new THREE.MeshBasicMaterial({ color: 0x2a0606 })); lumen.position.set(0, -.75, -2.9); lumen.scale.y = .6; g.add(lumen);
  // hard + soft palate (roof)
  const palate = new THREE.Mesh(new THREE.SphereGeometry(2, 48, 24, 0, Math.PI, 0, Math.PI / 2), wet(0xe08a8a, .45));
  palate.rotation.set(Math.PI / 2, 0, 0); palate.scale.set(1.05, 1.6, .55); palate.position.set(0, 1.0, 0); g.add(palate);
  // soft palate arch + pillars
  const archMat = wet(0xd56a6c, .35);
  function tube(points, r, mat) { const c = new THREE.CatmullRomCurve3(points.map(p => new THREE.Vector3(...p))); const m = new THREE.Mesh(new THREE.TubeGeometry(c, 48, r, 14, false), mat); g.add(m); return m; }
  // anterior pillars (palatoglossal) and posterior pillars (palatopharyngeal)
  tube([[-1.35, -1.1, -.7], [-1.25, -.2, -.9], [-.95, .55, -1.1], [-.45, .95, -1.2], [0, 1.05, -1.25], [.45, .95, -1.2], [.95, .55, -1.1], [1.25, -.2, -.9], [1.35, -1.1, -.7]], .17, archMat);
  tube([[-1.0, -1.3, -2.1], [-.95, -.3, -2.0], [-.75, .45, -1.85], [-.35, .8, -1.7], [0, .86, -1.65], [.35, .8, -1.7], [.75, .45, -1.85], [.95, -.3, -2.0], [1.0, -1.3, -2.1]], .13, wet(0xc4585b, .35));
  // velum sheet between arch and posterior pillar
  const velum = new THREE.Mesh(new THREE.SphereGeometry(1.2, 32, 16, 0, Math.PI, 0, Math.PI / 2), wet(0xd77073, .4, { side: THREE.DoubleSide }));
  velum.rotation.set(Math.PI / 2 + .3, 0, 0); velum.scale.set(1.0, .55, .45); velum.position.set(0, 1.0, -1.35); g.add(velum);
  // uvula
  const uvula = new THREE.Group(); uvula.position.set(0, .98, -1.38); g.add(uvula);
  const uv = new THREE.Mesh(new THREE.CapsuleGeometry(.13, .55, 8, 20), wet(0xd8686a, .3)); uv.position.y = -.4; uvula.add(uv);
  const uvTip = new THREE.Mesh(new THREE.SphereGeometry(.16, 20, 14), wet(0xd8686a, .3)); uvTip.position.y = -.72; uvula.add(uvTip);
  // tonsils
  const tonsilMat = wet(0xc86064, .5);
  const tonsils = [];
  for (const s of [-1, 1]) {
    const geo = noiseDisplace(new THREE.SphereGeometry(.42, 48, 36), .045, 9, s * 3);
    // crypts
    const col = []; const p = geo.attributes.position, v = new THREE.Vector3();
    for (let i = 0; i < p.count; i++) { v.fromBufferAttribute(p, i); const n = Math.sin(v.x * 23) * Math.sin(v.y * 19 + 1) * Math.sin(v.z * 21 + 2); const d = n > .55 ? .55 : 1; col.push(d, d * .97, d * .97); }
    geo.setAttribute('color', new THREE.Float32BufferAttribute(col, 3));
    const m = new THREE.Mesh(geo, tonsilMat.clone()); m.material.vertexColors = true;
    m.position.set(s * 1.08, -.45, -1.45); m.scale.set(.55, 1.0, .55); g.add(m); tonsils.push(m);
  }
  // tongue
  const tongue = new THREE.Mesh(noiseDisplace(new THREE.SphereGeometry(1.6, 64, 32), .015, 14), wet(0xd77a78, .55));
  tongue.scale.set(1.0, .42, 1.25); tongue.position.set(0, -1.55, -.2); g.add(tongue);
  const groove = new THREE.Mesh(new THREE.CapsuleGeometry(.03, 1.8, 4, 8), new THREE.MeshStandardMaterial({ color: 0xa04848, roughness: .6 }));
  groove.rotation.x = Math.PI / 2; groove.position.set(0, -.88, .2); g.add(groove);
  // tongue depressor
  const dep = new THREE.Mesh(new THREE.BoxGeometry(.55, .04, 3.4), new THREE.MeshStandardMaterial({ color: 0xe2c79a, roughness: .8 }));
  dep.position.set(0, -.86, 1.2); dep.rotation.x = -.06; g.add(dep);
  // teeth
  const toothMat = new THREE.MeshPhysicalMaterial({ color: 0xf5f0e6, roughness: .25, clearcoat: .6 });
  for (const [y, rot] of [[1.0, 1], [-1.48, -1]]) {
    for (let i = -5; i <= 5; i++) {
      const a = i * .17; const t = new THREE.Mesh(new THREE.BoxGeometry(.24, .34, .2, 2, 2, 2), toothMat);
      t.geometry = roundBox(t.geometry);
      t.position.set(Math.sin(a) * 1.75, y - Math.abs(i) * .015 * rot, 2.0 - (1 - Math.cos(a)) * 2.6); t.rotation.y = a; g.add(t);
    }
  }
  // gums
  const gumMat = wet(0xe08a8a, .5);
  const gu = new THREE.Mesh(new THREE.TorusGeometry(1.75, .16, 12, 48, Math.PI * .62), gumMat); gu.rotation.set(Math.PI / 2, 0, Math.PI * .19 + Math.PI); gu.position.set(0, 1.2, .25); gu.scale.set(1, 1.5, 1); g.add(gu);
  const gl = gu.clone(); gl.position.y = -1.68; g.add(gl);
  // lips
  const lip = new THREE.Mesh(new THREE.TorusGeometry(2.0, .35, 24, 64), new THREE.MeshPhysicalMaterial({ color: 0xb8706a, roughness: .45, clearcoat: .3 }));
  lip.scale.set(1.08, .86, .6); lip.position.set(0, -.25, 2.25); g.add(lip);
  const face = new THREE.Mesh(new THREE.RingGeometry(2.25, 6, 64), new THREE.MeshStandardMaterial({ color: 0xc69076, roughness: .7 }));
  face.scale.set(1.08, .86, 1); face.position.set(0, -.25, 2.3); g.add(face);

  // labels anchors
  const labels = [
    { t: 'Uvula', p: new THREE.Vector3(0, .05, -1.2) },
    { t: 'Tonsil (L)', p: new THREE.Vector3(1.08, -.45, -1.2) },
    { t: 'Tonsil (R)', p: new THREE.Vector3(-1.08, -.45, -1.2) },
    { t: 'Ant. pillar', p: new THREE.Vector3(1.45, .35, -.9) },
    { t: 'Soft palate', p: new THREE.Vector3(0, 1.1, -1.4) },
    { t: 'Post. pharyngeal wall', p: new THREE.Vector3(0, -.55, -2.9) },
    { t: 'Tongue', p: new THREE.Vector3(.95, -1.05, .9) },
  ];
  // Brodsky grade sizes (0–4): fraction of oropharyngeal width
  const gradeScale = [.25, .5, .78, 1.05, 1.32];
  const state = { grade: 1, aah: 0, red: false };
  function setGrade(gr) { state.grade = gr; }
  function update(t) {
    const s = gradeScale[state.grade];
    tonsils.forEach((m, i) => {
      const k = m.scale.x + ((.55 * s) - m.scale.x) * .15;
      m.scale.set(k, .95 + s * .12, k * 1.05);
      m.position.x = (i ? 1 : -1) * (1.18 - (k - .14) * .55);
      m.material.color.set(state.red ? 0xd8383c : 0xc86064);
      m.material.emissive.set(state.red ? 0x3a0000 : 0x000000);
    });
    const aah = .5 + .5 * Math.sin(t * 1.3);
    uvula.rotation.z = Math.sin(t * 2.1) * .04;
    uvula.position.y = .98 + aah * .12; uvula.scale.y = 1 - aah * .1;
    velum.position.y = 1.0 + aah * .08;
  }
  return { scene, cam, update, setGrade, state, labels };
}
function roundBox(geo) { // soften a box
  const p = geo.attributes.position, v = new THREE.Vector3();
  for (let i = 0; i < p.count; i++) { v.fromBufferAttribute(p, i); const l = v.length(); const n = v.clone().normalize().multiplyScalar(l * .6); v.lerp(n, .4); p.setXYZ(i, v.x, v.y, v.z); }
  geo.computeVertexNormals(); return geo;
}
