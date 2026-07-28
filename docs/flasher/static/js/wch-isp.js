/**
 * WCH ISP Protocol over WebUSB — CH32V307 bootloader communication
 * Implements: identify, set key, erase, program, verify, reset
 */
const WCH_VID = 0x4348;
const WCH_PID = 0x55e0;
const CMD_IDENTIFY  = 0xa1;
const CMD_ISP_END   = 0xa2;
const CMD_ISP_KEY   = 0xa3;
const CMD_ERASE     = 0xa4;
const CMD_PROGRAM   = 0xa5;
const CMD_VERIFY    = 0xa6;
const CMD_READ_CFG  = 0xa7;
const CMD_WRITE_CFG = 0xa8;
const SECTOR_SIZE   = 256;
const WRITE_CHUNK   = 56; // safe chunk for WCH ISP

class WchIsp {
  constructor() { this.device = null; this.epIn = null; this.epOut = null; this.xorKey = null; }

  async connect() {
    this.device = await navigator.usb.requestDevice({
      filters: [{ vendorId: WCH_VID, productId: WCH_PID }]
    });
    await this.device.open();
    await this.device.selectConfiguration(1);
    await this.device.claimInterface(0);
    const iface = this.device.configuration.interfaces[0].alternate;
    for (const ep of iface.endpoints) {
      if (ep.direction === 'in')  this.epIn  = ep.endpointNumber;
      if (ep.direction === 'out') this.epOut = ep.endpointNumber;
    }
    if (!this.epIn || !this.epOut) throw new Error('Cannot find USB bulk endpoints');
  }

  async _send(data) {
    const buf = data instanceof Uint8Array ? data : new Uint8Array(data);
    await this.device.transferOut(this.epOut, buf);
  }

  async _recv(len = 64) {
    const result = await this.device.transferIn(this.epIn, len);
    return new Uint8Array(result.data.buffer);
  }

  _buildCmd(cmd, payload = []) {
    const pLen = payload.length;
    const pkt = new Uint8Array(3 + pLen);
    pkt[0] = cmd; pkt[1] = pLen & 0xff; pkt[2] = (pLen >> 8) & 0xff;
    pkt.set(payload, 3);
    return pkt;
  }

  async identify() {
    await this._send(this._buildCmd(CMD_IDENTIFY, [0]));
    const r = await this._recv();
    if (r[0] !== CMD_IDENTIFY || r[1] !== 0) throw new Error('Identify failed');
    const chipId = r[4] | (r[5] << 8);
    const chipType = r[6];
    return { chipId, chipType, raw: r };
  }

  async readConfig() {
    const cfgMask = new Uint8Array([0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00]);
    await this._send(this._buildCmd(CMD_READ_CFG, cfgMask));
    const r = await this._recv(64);
    if (r[0] !== CMD_READ_CFG) throw new Error('Read config failed');
    const uid = Array.from(r.slice(18, 26)).map(b => b.toString(16).padStart(2,'0')).join(':');
    const blVer = `${r[14]}.${r[15]}.${r[16]}`;
    return { uid, blVer, raw: r };
  }

  async setKey(seed = null) {
    const key = seed || new Uint8Array(30);
    if (!seed) crypto.getRandomValues(key);
    await this._send(this._buildCmd(CMD_ISP_KEY, key));
    const r = await this._recv();
    if (r[0] !== CMD_ISP_KEY || r[1] !== 0) throw new Error('Set ISP key failed');
    // Derive XOR key from sum
    let sum = 0; for (const b of key) sum += b;
    this.xorKey = (sum & 0xff);
    return this.xorKey;
  }

  _xorEncode(data) {
    if (!this.xorKey) return data;
    const out = new Uint8Array(data.length);
    for (let i = 0; i < data.length; i++) out[i] = data[i] ^ this.xorKey;
    return out;
  }

  async erase(sectors) {
    const payload = new Uint8Array(4);
    payload[0] = sectors & 0xff; payload[1] = (sectors >> 8) & 0xff;
    payload[2] = (sectors >> 16) & 0xff; payload[3] = (sectors >> 24) & 0xff;
    await this._send(this._buildCmd(CMD_ERASE, payload));
    const r = await this._recv();
    if (r[0] !== CMD_ERASE || r[1] !== 0) throw new Error('Erase failed');
  }

  async program(address, firmware, onProgress) {
    const total = firmware.length;
    let offset = 0;
    while (offset < total) {
      const chunkLen = Math.min(WRITE_CHUNK, total - offset);
      const chunk = this._xorEncode(firmware.slice(offset, offset + chunkLen));
      const addrBytes = new Uint8Array(5);
      const addr = address + offset;
      addrBytes[0] = addr & 0xff; addrBytes[1] = (addr >> 8) & 0xff;
      addrBytes[2] = (addr >> 16) & 0xff; addrBytes[3] = (addr >> 24) & 0xff;
      addrBytes[4] = 0;
      const payload = new Uint8Array(5 + chunkLen);
      payload.set(addrBytes, 0); payload.set(chunk, 5);
      await this._send(this._buildCmd(CMD_PROGRAM, payload));
      const r = await this._recv();
      if (r[0] !== CMD_PROGRAM || r[1] !== 0) throw new Error(`Program failed at 0x${addr.toString(16)}`);
      offset += chunkLen;
      if (onProgress) onProgress(offset, total);
    }
  }

  async verify(address, firmware, onProgress) {
    const total = firmware.length;
    let offset = 0;
    while (offset < total) {
      const chunkLen = Math.min(WRITE_CHUNK, total - offset);
      const chunk = this._xorEncode(firmware.slice(offset, offset + chunkLen));
      const addrBytes = new Uint8Array(5);
      const addr = address + offset;
      addrBytes[0] = addr & 0xff; addrBytes[1] = (addr >> 8) & 0xff;
      addrBytes[2] = (addr >> 16) & 0xff; addrBytes[3] = (addr >> 24) & 0xff;
      addrBytes[4] = 0;
      const payload = new Uint8Array(5 + chunkLen);
      payload.set(addrBytes, 0); payload.set(chunk, 5);
      await this._send(this._buildCmd(CMD_VERIFY, payload));
      const r = await this._recv();
      if (r[0] !== CMD_VERIFY || r[1] !== 0) throw new Error(`Verify failed at 0x${addr.toString(16)}`);
      offset += chunkLen;
      if (onProgress) onProgress(offset, total);
    }
  }

  async reset() {
    await this._send(this._buildCmd(CMD_ISP_END, [1]));
    try { await this._recv(); } catch(e) { /* device may disconnect immediately */ }
  }

  async disconnect() {
    try { await this.device?.close(); } catch(e) {}
    this.device = null;
  }
}

window.WchIsp = WchIsp;
