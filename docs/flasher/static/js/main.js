/**
 * SayoFW Web Flasher — UI controller
 */
const FLASH_BASE = 0x08004000;
const FW_URL = 'firmware/sayofw_o3c.bin';
const MANIFEST_URL = 'manifest.json';

const $ = s => document.querySelector(s);
const log = (msg, cls = 'log-info') => {
  const c = $('#console');
  const d = document.createElement('div');
  d.className = cls;
  d.textContent = `[${new Date().toLocaleTimeString()}] ${msg}`;
  c.appendChild(d);
  c.scrollTop = c.scrollHeight;
};

let isp = null, fwData = null, manifest = null;

// State machine
function setState(step) {
  document.querySelectorAll('.step').forEach(s => {
    const n = +s.dataset.step;
    s.classList.toggle('step--active', n === step);
    s.classList.toggle('step--done', n < step);
    s.classList.toggle('step--disabled', n > step);
  });
}

// Browser check
function checkBrowser() {
  if (!navigator.usb) {
    $('#browser-warning').hidden = false;
    log('WebUSB not supported in this browser', 'log-err');
    return false;
  }
  $('#btn-connect').disabled = false;
  setState(1);
  log('Browser supports WebUSB ✓', 'log-ok');
  return true;
}

// Load manifest
async function loadManifest() {
  try {
    const r = await fetch(MANIFEST_URL);
    manifest = await r.json();
    $('#fw-name').textContent = manifest.name || 'SayoFW O3C';
    $('#fw-version').textContent = manifest.version || 'v0.1.0';
    $('#fw-desc').textContent = manifest.description || '';
    $('#badge-version').textContent = manifest.version || 'v0.1.0';
    log(`Manifest loaded: ${manifest.name} ${manifest.version}`);
  } catch(e) { log('Using default firmware config', 'log-warn'); }
}

// Connect
async function doConnect() {
  try {
    isp = new WchIsp();
    log('Requesting USB device…');
    await isp.connect();
    log('Connected to WCH bootloader', 'log-ok');
    setState(2);

    log('Identifying chip…');
    const id = await isp.identify();
    $('#chip-name').textContent = `CH32V (ID: 0x${id.chipId.toString(16)})`;
    $('#chip-flash').textContent = '256 KB';

    const cfg = await isp.readConfig();
    $('#chip-uid').textContent = cfg.uid;
    $('#chip-bl').textContent = `v${cfg.blVer}`;
    log(`Chip UID: ${cfg.uid}, Bootloader: v${cfg.blVer}`, 'log-ok');

    await isp.setKey();
    log('ISP key set ✓');

    $('#device-info').hidden = false;
    setState(3);
    $('#firmware-select').hidden = false;
  } catch(e) {
    log(`Connection failed: ${e.message}`, 'log-err');
    isp = null;
  }
}

// Load firmware
async function loadFirmware() {
  if (fwData) return fwData;
  log('Downloading firmware binary…');
  const r = await fetch(FW_URL);
  if (!r.ok) throw new Error(`Failed to fetch firmware: ${r.status}`);
  fwData = new Uint8Array(await r.arrayBuffer());

  // F6: refuse to flash the placeholder binary. The shipped manifest
  // ships a 1 KB "sayofw_o3c.bin" marker file (sha256 == "placeholder")
  // so the GitHub Pages site builds before the firmware does. Without
  // this guard, a user clicking "Flash" would erase their app region
  // and write 1 KB of `SAYOFW_PLACEHOLDER_v0.1.0\n` + 0xFF padding,
  // then the bootloader would refuse to boot. So bail out with a clear
  // error pointing at the docs.
  const sizeOk = fwData.length >= 1024 && fwData.length <= 256 * 1024;
  const six = fwData.slice(0, 26);
  const text = new TextDecoder('utf-8').decode(six);
  const isPlaceholder = /^SAYOFW_PLACEHOLDER/.test(text);
  if (isPlaceholder) {
    fwData = null;
    throw new Error(
      'Firmware is a placeholder build artifact. The repository has not ' +
      'yet published a real firmware binary. See docs/BUILD.md for how to ' +
      'produce one, or wait for a release tag. (F6)'
    );
  }
  if (!sizeOk) {
    fwData = null;
    throw new Error(
      `Firmware size ${fwData.length} bytes is outside the safe range ` +
      `[1 KB, 256 KB]. Refusing to flash.`
    );
  }

  $('#fw-size').textContent = `${(fwData.length / 1024).toFixed(1)} KB`;
  log(`Firmware loaded: ${fwData.length} bytes`, 'log-ok');
  return fwData;
}

// Flash
async function doFlash() {
  const prog = $('#flash-progress'), fill = $('#progress-fill');
  const label = $('#progress-label'), detail = $('#progress-detail');
  const speed = $('#progress-speed'), icon = $('#progress-icon');

  prog.hidden = false;
  $('#flash-result').hidden = true;
  $('#firmware-select').querySelector('.firmware-actions').style.display = 'none';

  const update = (pct, text) => {
    fill.style.width = pct + '%';
    detail.textContent = pct + '%';
    label.textContent = text;
  };

  try {
    const fw = await loadFirmware();
    const sectors = Math.ceil(fw.length / 256);

    update(5, 'Erasing flash…');
    log(`Erasing ${sectors} sectors…`);
    await isp.erase(sectors);
    log('Erase complete ✓', 'log-ok');

    update(10, 'Programming…');
    log(`Programming ${fw.length} bytes at 0x${FLASH_BASE.toString(16)}…`);
    const t0 = performance.now();
    await isp.program(FLASH_BASE, fw, (done, total) => {
      const pct = 10 + Math.round((done / total) * 50);
      update(pct, 'Programming…');
      const elapsed = (performance.now() - t0) / 1000;
      speed.textContent = elapsed > 0 ? `${(done / elapsed / 1024).toFixed(1)} KB/s` : '';
    });
    log('Program complete ✓', 'log-ok');

    update(65, 'Verifying…');
    log('Verifying written data…');
    await isp.verify(FLASH_BASE, fw, (done, total) => {
      const pct = 65 + Math.round((done / total) * 30);
      update(pct, 'Verifying…');
    });
    log('Verify complete ✓', 'log-ok');

    update(98, 'Resetting device…');
    await isp.reset();
    log('Device reset — flash complete!', 'log-ok');
    update(100, 'Done!');
    icon.textContent = '✅'; icon.classList.remove('spinner');

    $('#result-success').hidden = false;
    $('#flash-result').hidden = false;
  } catch(e) {
    log(`Flash error: ${e.message}`, 'log-err');
    icon.textContent = '❌'; icon.classList.remove('spinner');
    label.textContent = 'Failed';
    $('#error-message').textContent = e.message;
    $('#result-error').hidden = false;
    $('#flash-result').hidden = false;
  }
}

// Custom file
function handleCustomFile(e) {
  const file = e.target.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = () => {
    const bytes = new Uint8Array(reader.result);
    // F6: same placeholder/size guard as loadFirmware(), applied to a
    // user-chosen .bin so an honest typo can't brick their device.
    const sizeOk = bytes.length >= 1024 && bytes.length <= 256 * 1024;
    const head = bytes.slice(0, 26);
    const text = new TextDecoder('utf-8').decode(head);
    const isPlaceholder = /^SAYOFW_PLACEHOLDER/.test(text);
    if (isPlaceholder) {
      log(`Refused custom file: placeholder binary.`, 'log-err');
      fwData = null;
      return;
    }
    if (!sizeOk) {
      log(`Refused custom file: size ${bytes.length} B outside [1 KB, 256 KB].`, 'log-err');
      fwData = null;
      return;
    }
    fwData = bytes;
    $('#fw-name').textContent = file.name;
    $('#fw-size').textContent = `${(fwData.length / 1024).toFixed(1)} KB`;
    $('#fw-version').textContent = 'custom';
    log(`Custom firmware loaded: ${file.name} (${fwData.length} bytes)`, 'log-warn');
  };
  reader.readAsArrayBuffer(file);
}

// Init
document.addEventListener('DOMContentLoaded', () => {
  if (!checkBrowser()) return;
  loadManifest();
  $('#btn-connect').addEventListener('click', doConnect);
  $('#btn-flash').addEventListener('click', doFlash);
  $('#btn-retry').addEventListener('click', () => location.reload());
  $('#btn-clear-log').addEventListener('click', () => { $('#console').innerHTML = ''; });
  $('#file-input').addEventListener('change', handleCustomFile);
});
