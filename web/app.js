/* CineQ - giao diện web. Gọi REST API của server C++ cùng địa chỉ (/api/...). Định tuyến bằng #/đường-dẫn. */
'use strict';

const S = { user: null, token: '', cat: null, offset: 0, homeTab: 'NOW', date: '', filter: 'ALL', q: '', newsCat: '', refresh: null, grants: new Set(), change: 0 };
let B = null;  // trạng thái đang đặt vé
let P = { method: '', cash: 0 };  // trạng thái thanh toán

/* ---------- tiện ích ---------- */
const $ = (s, r = document) => r.querySelector(s);
const esc = v => String(v ?? '').replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
const money = n => Number(n || 0).toLocaleString('vi-VN') + ' đ';
const d2 = n => String(n).padStart(2, '0');
const nowSec = () => Math.floor(Date.now() / 1000) + S.offset;
const fmtT = ts => { const d = new Date(ts * 1000); return d2(d.getHours()) + ':' + d2(d.getMinutes()); };
const fmtD = ts => { const d = new Date(ts * 1000); return d2(d.getDate()) + '/' + d2(d.getMonth() + 1); };
const fmtDT = ts => fmtT(ts) + ' · ' + fmtD(ts) + '/' + new Date(ts * 1000).getFullYear();
const dayKey = ts => { const d = new Date(ts * 1000); return d.getFullYear() + '-' + d2(d.getMonth() + 1) + '-' + d2(d.getDate()); };
const WD = ['CN', 'T2', 'T3', 'T4', 'T5', 'T6', 'T7'];
const hue = s => { let h = 0; for (const c of String(s)) h = (h * 31 + c.codePointAt(0)) % 360; return h; };
const seatName = (r, c) => String.fromCharCode(65 + r) + (c + 1);
const isAdmin = () => !!S.user && String(S.user.role).toLowerCase() === 'admin';
const typeName = t => t === 'VIP' ? 'VIP' : 'Thường';
const METHODS = { CASH: ['Tiền mặt tại quầy', 'Thu tiền và thối lại ngay'], BANK: ['Chuyển khoản QR', 'Quét mã bằng ứng dụng ngân hàng'], WALLET: ['Ví điện tử', 'Quét mã bằng ví'], CARD: ['Thẻ ATM / Visa / Mastercard', 'Thanh toán qua cổng thẻ'] };
const STATUS = { HOLD: ['Chờ thanh toán', 'warn'], PAID: ['Đã thanh toán', 'ok'], CANCELLED: ['Đã hủy', 'bad'], EXPIRED: ['Hết hạn giữ ghế', 'bad'] };
const AGE = { T13: 13, T16: 16, T18: 18 };
const store = {
  get(k) { try { return localStorage.getItem(k) || ''; } catch { return ''; } },
  set(k, v) { try { v ? localStorage.setItem(k, v) : localStorage.removeItem(k); } catch { /* bỏ qua */ } }
};
const sleep = ms => new Promise(r => setTimeout(r, ms));
const qrSvg = text => { const q = qrcode(0, 'M'); q.addData(text); q.make(); return q.createSvgTag({ cellSize: 4, margin: 0, scalable: true }); };

let toastTimer;
function toast(msg, kind = '') {
  const t = $('#toast'); t.textContent = msg; t.className = 'toast ' + kind; t.hidden = false;
  clearTimeout(toastTimer); toastTimer = setTimeout(() => { t.hidden = true; }, 4500);
}

async function api(path, body) {
  const opt = { headers: {} };
  if (S.token) opt.headers['X-Token'] = S.token;
  if (body !== undefined) { opt.method = 'POST'; opt.headers['Content-Type'] = 'application/json'; opt.body = JSON.stringify(body); }
  let res;
  try { res = await fetch('/api' + path, opt); } catch { throw new Error('Không kết nối được máy chủ. Hãy kiểm tra server C++ đang chạy.'); }
  let data = {};
  try { data = await res.json(); } catch { /* không phải JSON */ }
  if (res.status === 401 && S.token && !path.startsWith('/login')) { S.token = ''; S.user = null; store.set('cq_token', ''); }
  if (!res.ok) { const e = new Error(data.error || ('Lỗi ' + res.status)); e.status = res.status; throw e; }
  return data;
}

/* ---------- thành phần giao diện & quản lý ảnh poster ---------- */
const DEFAULT_POSTERS = {
  // 1. Nữ hoàng nước mắt (Link mới theo yêu cầu của bạn)
  'Nữ hoàng nước mắt': 'https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcQTGp1CIvDTtcUOxbssufHa0s3U-0zUWaDdcgGrBM-Vp-oyISPNdSLLGqVJ&s=10',
  // 2. Nhiệm vụ: Bất khả thi - Nghiệp báo cuối cùng
  'Nhiệm vụ: Bất khả thi - Nghiệp báo cuối cùng': 'https://image.tmdb.org/t/p/w500/NNxYkU70HPurnNCSiCjYAmacwm.jpg',
  // 3. Tuyển thủ Dê: “Mùi” vị chiến thắng
  'Tuyển thủ Dê: “Mùi” vị chiến thắng': 'https://image.tmdb.org/t/p/w500/wfuqMlaExcoYiUEvKfVpUTt1v4u.jpg',
  // 4. Ám ảnh
  'Ám ảnh': 'https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcS88h68oOQDGXPW5qWJQ9sYs7rI6tWt1X7oVVh91dNWXw&s=10',
  // 5. Hố đen tử thần
  'Hố đen tử thần': 'https://image.tmdb.org/t/p/w500/gEU2QniE6E77NI6lCU6MxlNBvIx.jpg',
  'Hành Tinh Cuối Cùng': 'https://image.tmdb.org/t/p/w500/gEU2QniE6E77NI6lCU6MxlNBvIx.jpg',
  // 6. Bố già (2021)
  'Bố già (2021)': 'https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcTzIM1pI8am-txu5qG9PnlsKfP96VB9UcA6Qawq2GmJlg&s=10',
  'Bố già': 'https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcTzIM1pI8am-txu5qG9PnlsKfP96VB9UcA6Qawq2GmJlg&s=10',
  'Tiệm Bánh Hạnh Phúc': 'https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcTzIM1pI8am-txu5qG9PnlsKfP96VB9UcA6Qawq2GmJlg&s=10'
};

// Link dự phòng tự động nếu link chính gặp sự cố kết nối
const FALLBACK_POSTERS = {
  'Nữ hoàng nước mắt': 'https://upload.wikimedia.org/wikipedia/en/6/62/Queen_of_Tears_poster.png',
  'Ám ảnh': 'https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcS88h68oOQDGXPW5qWJQ9sYs7rI6tWt1X7oVVh91dNWXw&s=10',
  'Bố già (2021)': 'https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcTzIM1pI8am-txu5qG9PnlsKfP96VB9UcA6Qawq2GmJlg&s=10'
};

function getCustomPosters() {
  try { return JSON.parse(store.get('cq_posters') || '{}'); } catch { return {}; }
}
function saveCustomPoster(title, url) {
  const map = getCustomPosters();
  if (url) map[title] = url; else delete map[title];
  store.set('cq_posters', JSON.stringify(map));
}
function getPosterUrl(title) {
  const custom = getCustomPosters();
  return custom[title] || DEFAULT_POSTERS[title] || '';
}

// Tự động kiểm tra tỷ lệ ảnh khi tải xong: nếu ảnh ngang (landscape) thì bật chế độ vừa khung (fit-contain) kèm nền mờ để không bị cắt mất hình
window.onPosterLoad = function(img) {
  const box = img.parentElement;
  if (!box) return;
  if (img.naturalWidth > img.naturalHeight * 1.05) {
    box.classList.add('is-landscape');
  } else {
    box.classList.remove('is-landscape');
  }
};

window.onPosterError = function(img) {
  const box = img.parentElement;
  const fb = img.dataset.fallback;
  if (fb && img.src !== fb) {
    img.src = fb;
    const bg = box && box.querySelector('.poster-bg');
    if (bg) bg.style.backgroundImage = `url("${fb.replace(/"/g, '\\"')}")`;
    return;
  }
  if (box) box.classList.remove('img', 'is-landscape');
  const bg = box && box.querySelector('.poster-bg');
  if (bg) bg.remove();
  img.remove();
};

function posterImgTag(title) {
  const url = getPosterUrl(title);
  if (!url) return '';
  const fb = FALLBACK_POSTERS[title] || '';
  return `<div class="poster-bg" style="background-image:url('${esc(url)}')"></div>
    <img class="poster-media" src="${esc(url)}" data-fallback="${esc(fb)}" alt="${esc(title)}" loading="lazy" onload="onPosterLoad(this)" onerror="onPosterError(this)">`;
}

function poster(m) {
  const hasImg = !!getPosterUrl(m.title);
  return `<div class="poster${hasImg ? ' img' : ''}" style="--h:${hue(m.title)}">
    ${posterImgTag(m.title)}
    <span class="age ${esc(m.rating)}">${esc(m.rating)}</span>
    ${m.status === 'SOON' ? '<span class="soon">Sắp chiếu</span>' : ''}
    <div class="t">${esc(m.title)}</div>
    <div class="g">${esc(m.genre)}</div>
  </div>`;
}

const miniPoster = title => {
  const hasImg = !!getPosterUrl(title);
  return `<div class="poster mini${hasImg ? ' img' : ''}" style="--h:${hue(title)}">
    ${posterImgTag(title)}
    <div class="t">${esc(title)}</div>
  </div>`;
};

const movieOf = id => S.cat.movies.find(m => m.id == id);

function draw(active, html) {
  const links = [['#/', 'Phim', 'home'], ['#/schedule', 'Lịch chiếu', 'schedule'], ['#/news', 'Tin tức', 'news']];
  if (S.user) links.push(['#/orders', isAdmin() ? 'Đơn hàng' : 'Vé của tôi', 'orders']);
  if (isAdmin()) links.push(['#/admin/checkin', 'Soát vé', 'checkin'], ['#/admin/reports', 'Báo cáo', 'reports'], ['#/admin/catalog', 'Phim và suất', 'catalog']);
  $('#app').innerHTML = `
  <header class="nav"><div class="wrap">
    <a class="logo" href="#/"><i></i>CineQ</a>
    <nav class="links">${links.map(([h, l, k]) => `<a href="${h}" ${active === k ? 'aria-current="page"' : ''}>${l}</a>`).join('')}</nav>
    <div class="navr">
      <button class="btn sm ghost" data-act="theme" aria-label="Đổi giao diện sáng tối">Sáng/Tối</button>
      ${S.user ? `<span class="avatar" title="${esc(S.user.fullName)}">${esc(S.user.fullName.trim().charAt(0).toUpperCase())}</span>
        <span class="hide-sm"><b>${esc(S.user.fullName)}</b>${isAdmin() ? ' <span class="badge gold">Admin</span>' : ''}</span>
        <button class="btn sm" data-act="logout">Đăng xuất</button>`
        : `<button class="btn sm" data-act="login">Đăng nhập</button><button class="btn sm primary" data-act="register">Đăng ký</button>`}
    </div></div></header>
  <main>${html}</main>
  <footer class="foot"><div class="wrap"><span>CineQ · Đặt vé xem phim</span><span class="mono">${new Date().getFullYear()}</span></div></footer>`;
  tick();
}
const crumbs = items => `<div class="wrap"><div class="crumbs" style="padding-top:20px">${items.map(([h, l]) => h ? `<a href="${h}">${esc(l)}</a>` : `<span>${esc(l)}</span>`).join('<span>/</span>')}</div></div>`;

function modal(html, wide) {
  const root = $('#modal-root');
  root.innerHTML = `<div class="overlay" role="dialog" aria-modal="true"><div class="dialog ${wide ? 'wide' : ''}">${html}</div></div>`;
  const ov = root.firstChild;
  const close = () => { root.innerHTML = ''; document.removeEventListener('keydown', onKey); };
  const onKey = e => { if (e.key === 'Escape') close(); };
  document.addEventListener('keydown', onKey);
  ov.addEventListener('mousedown', e => { if (e.target === ov) close(); });
  root.querySelectorAll('[data-close]').forEach(b => b.addEventListener('click', close));
  const f = root.querySelector('input:not([type=hidden]),textarea,select'); if (f) f.focus();
  return { root, close };
}
function confirmDlg(title, html, okLabel, onOk, danger = true) {
  const m = modal(`<div class="dlg-h">${esc(title)}</div><div>${html}</div>
    <div class="dlg-act"><button class="btn" data-close>Quay lại</button><button class="btn ${danger ? 'warn' : 'primary'}" id="ok">${esc(okLabel)}</button></div>`);
  m.root.querySelector('#ok').onclick = async () => { m.close(); try { await onOk(); } catch (e) { toast(e.message, 'bad'); } };
}

/* ---------- đăng nhập ---------- */
function authModal(mode, then) {
  const reg = mode === 'register';
  const m = modal(`<div class="dlg-h">${reg ? 'Tạo tài khoản' : 'Đăng nhập'}</div>
    <form id="af">
      ${reg ? '<div class="field"><label class="lab" for="a-full">Họ tên</label><input class="in" id="a-full" maxlength="40" autocomplete="name"></div>' : ''}
      <div class="field"><label class="lab" for="a-user">Tên đăng nhập</label><input class="in" id="a-user" autocomplete="username"></div>
      <div class="field"><label class="lab" for="a-pass">Mật khẩu</label><input class="in" id="a-pass" type="password" autocomplete="${reg ? 'new-password' : 'current-password'}"></div>
      ${reg ? '<div class="field"><label class="lab" for="a-conf">Nhập lại mật khẩu</label><input class="in" id="a-conf" type="password" autocomplete="new-password"></div>' : ''}
      <div class="err" id="a-err"></div>
      <button class="btn primary block lg" type="submit">${reg ? 'Đăng ký' : 'Đăng nhập'}</button>
    </form>
    <div class="row" style="margin-top:14px;justify-content:space-between"><button class="link" id="a-sw">${reg ? 'Đã có tài khoản' : 'Tạo tài khoản mới'}</button>
      ${reg ? '' : '<span class="row"><button class="btn sm" data-fill="khach,khach123">Khách mẫu</button><button class="btn sm" data-fill="admin,admin123">Admin mẫu</button></span>'}</div>`);
  const r = m.root, g = id => (r.querySelector(id) || {}).value || '';
  r.querySelector('#a-sw').onclick = () => authModal(reg ? 'login' : 'register', then);
  r.querySelectorAll('[data-fill]').forEach(b => b.onclick = () => { const [u, p] = b.dataset.fill.split(','); r.querySelector('#a-user').value = u; r.querySelector('#a-pass').value = p; });
  r.querySelector('#af').onsubmit = async e => {
    e.preventDefault();
    try {
      const d = reg ? await api('/register', { fullName: g('#a-full'), username: g('#a-user'), password: g('#a-pass'), confirm: g('#a-conf') })
                    : await api('/login', { username: g('#a-user'), password: g('#a-pass') });
      S.token = d.token; S.user = d.user; store.set('cq_token', d.token);
      m.close(); toast('Xin chào ' + d.user.fullName + '.');
      if (then) await then(); else await route();
    } catch (er) { r.querySelector('#a-err').textContent = er.message; }
  };
}
function requireLogin(then) { if (S.user) return then(); authModal('login', then); }

/* ---------- trang chủ, chi tiết phim, lịch chiếu ---------- */
function movieCard(m) {
  return `<a class="mv" href="#/movie/${m.id}">${poster(m)}<div><h3>${esc(m.title)}</h3><div class="meta"><span>${esc(m.genre)}</span><span>${m.minutes} phút</span></div></div></a>`;
}
async function vHome() {
  const movies = S.cat.movies;
  const now = movies.filter(m => m.status === 'NOW'), soon = movies.filter(m => m.status === 'SOON');
  const feat = [...now].sort((a, b) => b.shows - a.shows)[0];
  const list = S.homeTab === 'NOW' ? now : soon;
  draw('home', `${feat ? `<section class="hero" style="--h:${hue(feat.title)}"><div class="wrap"><div>
      <span class="badge gold">Phim nổi bật</span><h1>${esc(feat.title)}</h1>
      <div class="meta"><span>${esc(feat.genre)}</span><span>${feat.minutes} phút</span><span class="badge">${esc(feat.rating)}</span></div>
      <p class="syn">${esc(feat.synopsis)}</p><a class="btn primary lg" href="#/movie/${feat.id}">Mua vé</a></div>${poster(feat)}</div></section>` : ''}
    <section class="sec wrap"><div class="sec-h"><h2>Phim</h2>
      <div class="seg"><button data-act="homeTab" data-v="NOW" aria-pressed="${S.homeTab === 'NOW'}">Đang chiếu (${now.length})</button><button data-act="homeTab" data-v="SOON" aria-pressed="${S.homeTab === 'SOON'}">Sắp chiếu (${soon.length})</button></div></div>
      ${list.length ? `<div class="movies">${list.map(movieCard).join('')}</div>` : '<div class="empty">Chưa có phim trong mục này.</div>'}</section>`);
}

function timeBtn(s, withRoom) {
  const free = s.freeNormal + s.freeVip;
  let cls = '', sub;
  if (!s.open) { cls = 'closed'; sub = 'Đã đóng'; }
  else if (free === 0) { cls = 'out'; sub = 'Hết vé' + (s.waiting ? ` · ${s.waiting} chờ` : ''); }
  else { sub = `Còn ${free}`; if (free <= 8) cls = 'low'; }
  if (withRoom) sub = esc(s.room) + ' · ' + sub;
  return `<a class="time ${cls}" href="#/book/${s.id}"><b>${fmtT(s.start)}</b><span>${sub}</span></a>`;
}
function dateStrip(keys, stamp) {
  const today = dayKey(nowSec()), tomorrow = dayKey(nowSec() + 86400);
  return `<div class="dates">${keys.map(k => {
    const d = new Date(stamp[k] * 1000);
    const lab = k === today ? 'Hôm nay' : k === tomorrow ? 'Ngày mai' : WD[d.getDay()];
    return `<button class="date" data-act="date" data-v="${k}" aria-pressed="${S.date === k}"><span>${lab}</span><b>${d2(d.getDate())}/${d2(d.getMonth() + 1)}</b></button>`;
  }).join('')}</div>`;
}
function groupDates(items) {
  const keys = [], stamp = {};
  for (const s of items) { const k = dayKey(s.start); if (!stamp[k]) { stamp[k] = s.start; keys.push(k); } }
  if (!keys.includes(S.date)) S.date = keys[0] || '';
  return { keys, stamp };
}

async function vMovie(id) {
  const m = movieOf(id);
  if (!m) return notFound();
  const load = async () => {
    const { items } = await api('/shows?movie=' + id);
    const { keys, stamp } = groupDates(items);
    const day = items.filter(s => dayKey(s.start) === S.date);
    const rooms = [...new Set(day.map(s => s.room))].sort();
    return `${keys.length ? dateStrip(keys, stamp) : ''}
      ${day.length ? rooms.map(r => `<div class="room"><h4>${esc(r)}</h4><div class="times">${day.filter(s => s.room === r).map(s => timeBtn(s)).join('')}</div></div>`).join('')
        : `<div class="empty">${m.status === 'SOON' ? 'Phim chưa mở bán vé.' : 'Chưa có suất chiếu.'}</div>`}`;
  };
  draw('home', `${crumbs([['#/', 'Phim'], [null, m.title]])}<section class="wrap" style="padding-block:10px 20px"><div class="detail">${poster(m)}<div>
    <h1>${esc(m.title)}</h1>
    <div class="meta"><span>${esc(m.genre)}</span><span>${m.minutes} phút</span><span class="badge ${AGE[m.rating] ? 'gold' : ''}">${esc(m.rating)}</span>${m.status === 'SOON' ? '<span class="badge">Sắp chiếu</span>' : ''}</div>
    <p class="muted" style="margin-top:14px;max-width:64ch;font-size:16px">${esc(m.synopsis)}</p>
    <div id="shows">${await load()}</div></div></div></section>`);
  S.refresh = async () => { const el = $('#shows'); if (el) el.innerHTML = await load(); };
}

async function vSchedule() {
  const load = async () => {
    const { items } = await api('/shows');
    const { keys, stamp } = groupDates(items);
    const day = items.filter(s => dayKey(s.start) === S.date);
    const ids = [...new Set(day.map(s => s.movieId))];
    return `${keys.length ? dateStrip(keys, stamp) : ''}${ids.length ? ids.map(id => {
      const m = movieOf(id); if (!m) return '';
      return `<div class="card sched-card"><a class="sched-poster" href="#/movie/${m.id}">${miniPoster(m.title)}</a>
        <div><h3 style="font:700 18px var(--body);text-transform:none;letter-spacing:0"><a href="#/movie/${m.id}" style="text-decoration:none">${esc(m.title)}</a></h3>
        <div class="meta" style="margin:2px 0 12px"><span>${esc(m.genre)}</span><span>${m.minutes} phút</span><span class="badge">${esc(m.rating)}</span></div>
        <div class="times">${day.filter(s => s.movieId === id).map(s => timeBtn(s, true)).join('')}</div></div></div>`;
    }).join('') : '<div class="empty">Chưa có suất chiếu.</div>'}`;
  };
  draw('schedule', `<section class="sec wrap"><div class="sec-h"><h2>Lịch chiếu</h2></div><div id="shows">${await load()}</div></section>`);
  S.refresh = async () => { const el = $('#shows'); if (el) el.innerHTML = await load(); };
}

function notFound() {
  draw('', `<section class="sec wrap"><div class="empty"><p>Không tìm thấy nội dung này.</p><p style="margin-top:14px"><a class="btn primary" href="#/">Về trang chủ</a></p></div></section>`);
}

/* ---------- đặt vé: chọn ghế, bắp nước ---------- */
const comboName = id => (S.cat.combos.find(c => c.id === id) || {}).name || id;
const comboPrice = id => (S.cat.combos.find(c => c.id === id) || {}).price || 0;
const comboPhotos = {
  BAP: 'https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcTAqMnW2WEwy4-JbCI7Oua99gynxj_Donlbc9xN5BSGYA7WiyWo9VqlVduk&s=10',
  BAPPM: 'https://cellphones.com.vn/sforum/wp-content/uploads/2023/07/gia-bap-nuoc-cgv-1.jpg',
  NUOC: 'https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcTxn5OOo-hnetOplbz5Z_ZJ4DnTttILmfu9UA_c9ZVGyh6lWoAB_lc8Km_m&s=10',
  C1: 'https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcSlXcSps0E_w_on5gKc5Qie4W1V1giahZyZfBD2baV1uMAYWS0i72aGd3EL&s=10',
  C2: 'https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcRHrgP84pnGJyYcECnCG0mtHbxgkQYrhMkiPOr1zU7cLIDjjLmcvrwt65fr&s=10'
};
const selKey = (r, c) => r + ',' + c;
const isSel = (r, c) => B.sel.some(p => p.row === r && p.col === c);
const steps = n => `<div class="steps">${['Chọn ghế', 'Bắp nước và ưu đãi', 'Thanh toán'].map((l, i) =>
  `<div class="step ${n === i + 1 ? 'on' : n > i + 1 ? 'done' : ''}"><i>${n > i + 1 ? '✓' : i + 1}</i>${l}</div>`).join('')}</div>`;

function seatMapHtml() {
  const d = B.data, cols = d.show.cols, aisleAt = cols >= 8 ? Math.floor(cols / 2) : 0;
  const tmpl = aisleAt ? `22px repeat(${aisleAt},var(--cell)) 16px repeat(${cols - aisleAt},var(--cell))` : `22px repeat(${cols},var(--cell))`;
  let g = `<div class="seatgrid" style="grid-template-columns:${tmpl}" role="group" aria-label="Sơ đồ ghế">`;
  d.seats.forEach((row, r) => {
    g += `<div class="rl">${String.fromCharCode(65 + r)}</div>`;
    row.forEach((s, c) => {
      if (aisleAt && c === aisleAt) g += '<i class="aisle"></i>';
      const on = isSel(r, c);
      g += `<button class="seat ${s.type === 'VIP' ? 'vip' : ''} ${on ? 'sel' : ''}" data-act="seat" data-r="${r}" data-c="${c}" ${s.taken ? 'disabled' : ''}
        aria-label="Ghế ${seatName(r, c)} ${typeName(s.type)} ${s.taken ? 'đã bán' : 'còn trống'}" aria-pressed="${on}">${seatName(r, c)}</button>`;
    });
  });
  return g + `</div><div class="legend"><span><i class="seat"></i>Thường ${money(d.show.priceNormal)}</span><span><i class="seat vip"></i>VIP ${money(d.show.priceVip)}</span><span><i class="seat" style="background:var(--taken);border-color:var(--taken)"></i>Đã bán</span><span><i class="seat sel"></i>Đang chọn</span></div>`;
}

function queueChain(q) {
  if (!q.length) return '';
  return `<div class="chain-scroll"><div class="chain">${q.map((c, i) => `${i ? '<div class="arrow" aria-hidden="true">⇄</div>' : ''}
    <div class="slot"><div class="ptrs">${i === 0 ? '<span class="ptr">HEAD</span>' : ''}${i === q.length - 1 ? '<span class="ptr tail">TAIL</span>' : ''}</div>
      <div class="node"><div class="pn">${i === 0 ? '∅' : '←'}</div><div class="nd"><small>#${i + 1}</small><b>${esc(c.name)}</b><span class="badge ${c.type === 'VIP' ? 'gold' : ''}">${typeName(c.type)}</span>
        ${c.canLeave ? `<button class="btn sm" data-act="leaveQ" data-id="${c.id}">Rời hàng</button>` : ''}</div><div class="pn">${i === q.length - 1 ? '∅' : '→'}</div></div></div>`).join('')}</div></div>`;
}
function waitCard() {
  const d = B.data, s = d.show, out = [];
  if (s.freeNormal === 0) out.push('NORMAL');
  if (s.freeVip === 0) out.push('VIP');
  if (!out.length && !d.queue.length) return '';
  const mine = d.queue.some(c => c.mine);
  return `<div class="card"><div class="sec-h" style="margin-bottom:12px"><h3 style="font-size:22px">Danh sách chờ</h3>
    ${!mine && out.length && s.open ? `<div class="row">${out.map(t => `<button class="btn sm primary" data-act="joinQ" data-t="${t}">Chờ ghế ${typeName(t)}</button>`).join('')}</div>` : ''}</div>
    ${d.queue.length ? queueChain(d.queue) : '<div class="muted">Chưa có ai chờ.</div>'}</div>`;
}

function stepSeats() {
  const d = B.data, s = d.show, max = S.cat.rules.maxSeats;
  const free = { NORMAL: s.freeNormal, VIP: s.freeVip };
  return `<div class="card"><div class="sec-h" style="margin-bottom:6px"><h3 style="font-size:24px">Chọn ghế</h3><span class="muted">${B.sel.length}/${max} ghế</span></div>
    <div class="screen">MÀN HÌNH</div><div class="seatwrap">${seatMapHtml()}</div>
    <div class="quick"><span class="lab">Chọn nhanh</span>
      <div class="qty"><button data-act="cnt" data-d="-1" aria-label="Giảm">−</button><output>${B.count}</output><button data-act="cnt" data-d="1" aria-label="Tăng">+</button></div>
      <div class="seg"><button data-act="qtype" data-v="NORMAL" aria-pressed="${B.type === 'NORMAL'}">Thường</button><button data-act="qtype" data-v="VIP" aria-pressed="${B.type === 'VIP'}">VIP</button></div>
      <button class="btn" data-act="auto" ${free[B.type] < 1 ? 'disabled' : ''}>Chọn ghế liền kề</button>
      ${B.sel.length ? '<button class="link" data-act="clearSel">Bỏ chọn</button>' : ''}</div></div>`;
}

function stepExtras() {
  const m = movieOf(B.data.show.movieId), age = m && AGE[m.rating];
  return `<div class="card"><h3 style="font-size:24px;margin-bottom:6px">Bắp nước</h3>
    <div class="combo-grid">${S.cat.combos.map(c => `<div class="combo-card"><img class="combo-art" src="${esc(comboPhotos[c.id] || comboPhotos.C1)}" alt="${esc(c.name)}" loading="lazy"><div class="combo-info"><b>${esc(c.name)}</b><span class="p">${money(c.price)}</span>
      <div class="qty"><button data-act="combo" data-id="${esc(c.id)}" data-d="-1" aria-label="Giảm">−</button><output>${B.combos[c.id] || 0}</output><button data-act="combo" data-id="${esc(c.id)}" data-d="1" aria-label="Tăng">+</button></div></div></div>`).join('')}</div></div>
    <div class="card"><h3 style="font-size:24px;margin-bottom:12px">Mã giảm giá</h3>
      ${B.promoOk ? `<div class="alert ok"><span><b>${esc(B.promoOk)}</b> · ${esc(B.quote && B.quote.promoNote || '')}</span><button class="btn sm" data-act="promoClear">Bỏ mã</button></div>`
        : `<div class="promo"><input class="in" id="promo" placeholder="Nhập mã" value="${esc(B.promoText)}" data-in="promo" autocomplete="off"><button class="btn" data-act="promo">Áp dụng</button></div><div class="err" id="promo-err">${esc(B.promoErr)}</div>
          <div class="chips" style="margin-top:6px">${S.cat.promos.map(p => `<button class="chip" data-act="promoPick" data-v="${esc(p.code)}" title="${esc(p.desc)}">${esc(p.code)}</button>`).join('')}</div>
          <div class="muted" style="margin-top:8px;font-size:13px">${S.cat.promos.map(p => `<div><b>${esc(p.code)}</b>: ${esc(p.desc)}</div>`).join('')}</div>`}</div>
    ${age ? `<div class="card"><label class="chk"><input type="checkbox" id="age" data-in="age" ${B.age ? 'checked' : ''}><span>Tôi xác nhận người xem đủ ${age} tuổi trở lên theo phân loại ${esc(m.rating)} của phim.</span></label></div>` : ''}`;
}

function sideHtml() {
  const d = B.data, s = d.show, m = movieOf(s.movieId) || { title: s.movie, rating: '' };
  const q = B.quote || { ticketsTotal: 0, comboTotal: 0, discount: 0, total: 0 };
  const cnt = { NORMAL: 0, VIP: 0 };
  B.sel.forEach(p => cnt[d.seats[p.row][p.col].type]++);
  const combos = Object.entries(B.combos).filter(([, n]) => n > 0);
  const hasSel = B.sel.length > 0;
  return `<div class="sumhead">${miniPoster(m.title)}<div><h3>${esc(m.title)}</h3><p>${fmtT(s.start)} · ${fmtD(s.start)} · ${esc(s.room)}</p><p><span class="badge">${esc(m.rating)}</span></p></div></div>
    ${B.sel.length ? `<div class="chips">${B.sel.map(p => `<span class="chip ${d.seats[p.row][p.col].type === 'VIP' ? 'vip' : ''}">${seatName(p.row, p.col)}</span>`).join('')}</div>` : '<div class="muted">Chưa chọn ghế.</div>'}
    <div class="lines">
      ${cnt.NORMAL ? `<div><span>Ghế thường × ${cnt.NORMAL}</span><span>${money(cnt.NORMAL * s.priceNormal)}</span></div>` : ''}
      ${cnt.VIP ? `<div><span>Ghế VIP × ${cnt.VIP}</span><span>${money(cnt.VIP * s.priceVip)}</span></div>` : ''}
      ${combos.map(([id, n]) => `<div><span>${esc(comboName(id))} × ${n}</span><span>${money(comboPrice(id) * n)}</span></div>`).join('')}
      ${q.discount ? `<div class="off"><span>Giảm giá ${esc(B.promoOk)}</span><span>− ${money(q.discount)}</span></div>` : ''}
      <div class="tot"><span>Tổng cộng</span><b>${money(q.total)}</b></div></div>
    ${isAdmin() ? `<div class="field"><label class="lab" for="cname">Tên khách</label><input class="in" id="cname" data-in="cname" maxlength="40" value="${esc(B.name)}" placeholder="Khách lẻ"></div>` : ''}
    ${B.step === 1
      ? `<button class="btn primary block lg" data-act="step2" ${hasSel ? '' : 'disabled'}>Tiếp tục</button>`
      : `<button class="btn primary block lg" data-act="hold" ${hasSel ? '' : 'disabled'}>Giữ ghế và thanh toán</button><button class="btn block" style="margin-top:8px" data-act="step1">Quay lại chọn ghế</button>`}`;
}

function bookPaint() {
  const main = $('#bmain'), side = $('#bside'), wait = $('#bwait');
  if (!main) return;
  main.innerHTML = B.step === 1 ? stepSeats() : stepExtras();
  side.innerHTML = sideHtml();
  wait.innerHTML = B.step === 1 ? waitCard() : '';
  const stp = $('#bsteps'); if (stp) stp.innerHTML = steps(B.step);
}

async function refreshQuote() {
  if (!B.sel.length) { B.quote = null; return; }
  const combos = {}; Object.entries(B.combos).forEach(([k, v]) => { if (v > 0) combos[k] = v; });
  try {
    B.quote = await api('/quote', { showId: B.showId, seats: B.sel.map(p => ({ row: p.row, col: p.col })), combos, promo: B.promoOk || '' });
  } catch (e) {
    if (B.promoOk) { B.promoOk = ''; B.promoErr = e.message; toast(e.message, 'bad'); return refreshQuote(); }
    B.quote = null;
  }
}
async function loadShow(id) { return api('/show?id=' + id); }

async function vBook(id) {
  id = Number(id);
  const data = await loadShow(id);
  if (!B || B.showId !== id) B = { showId: id, sel: [], combos: {}, promoOk: '', promoText: '', promoErr: '', quote: null, step: 1, count: 2, type: 'NORMAL', name: '', age: false };
  B.data = data;
  B.sel = B.sel.filter(p => !data.seats[p.row][p.col].taken);
  const s = data.show, m = movieOf(s.movieId);
  if (!s.open) {
    draw('schedule', `${crumbs([['#/', 'Phim'], [m ? '#/movie/' + m.id : null, s.movie], [null, 'Đặt vé']])}<section class="sec wrap"><div class="alert warn"><span>Suất chiếu này đã đóng bán vé.</span><a class="btn" href="#/schedule">Xem suất khác</a></div></section>`);
    return;
  }
  await refreshQuote();
  draw('schedule', `${crumbs([['#/', 'Phim'], [m ? '#/movie/' + m.id : null, s.movie], [null, 'Đặt vé']])}
    <section class="wrap" style="padding-block:6px 10px"><div id="bsteps"></div>
    <div class="book"><div><div id="bmain"></div><div id="bwait" style="margin-top:16px"></div></div><aside class="side card" id="bside"></aside></div></section>`);
  bookPaint();
  S.refresh = async () => {
    const fresh = await loadShow(id);
    const lost = B.sel.filter(p => fresh.seats[p.row][p.col].taken);
    B.data = fresh;
    if (lost.length) { B.sel = B.sel.filter(p => !fresh.seats[p.row][p.col].taken); toast('Ghế ' + lost.map(p => seatName(p.row, p.col)).join(', ') + ' vừa có người đặt.', 'bad'); await refreshQuote(); }
    if (B.step === 1 && !$('#bmain .quick:focus-within')) bookPaint(); else { $('#bside').innerHTML = sideHtml(); $('#bwait').innerHTML = B.step === 1 ? waitCard() : ''; }
  };
}

async function holdOrder() {
  const m = movieOf(B.data.show.movieId);
  if (m && AGE[m.rating] && !B.age) { toast('Hãy xác nhận độ tuổi xem phim ở bước Bắp nước và ưu đãi.', 'bad'); return; }
  const name = isAdmin() ? (B.name.trim() || 'Khách lẻ') : S.user.fullName;
  const combos = {}; Object.entries(B.combos).forEach(([k, v]) => { if (v > 0) combos[k] = v; });
  try {
    const d = await api('/orders', { showId: B.showId, name, seats: B.sel.map(p => ({ row: p.row, col: p.col })), combos, promo: B.promoOk || '' });
    B = null; P = { method: '', cash: 0 };
    location.hash = '#/pay/' + d.order.id;
  } catch (e) {
    toast(e.message, 'bad');
    B.data = await loadShow(B.showId); B.sel = B.sel.filter(p => !B.data.seats[p.row][p.col].taken); await refreshQuote(); bookPaint();
  }
}

/* ---------- thanh toán, vé điện tử, đơn hàng ---------- */
async function loadOrders() { return (await api('/orders')).items; }
const orderLines = o => `<div class="lines">
  <div><span>Vé (${o.tickets.length} ghế)</span><span>${money(o.ticketsTotal)}</span></div>
  ${o.combos.map(c => `<div><span>${esc(c.name)} × ${c.qty}</span><span>${money(c.price * c.qty)}</span></div>`).join('')}
  ${o.discount ? `<div class="off"><span>Giảm giá ${esc(o.promo)}</span><span>− ${money(o.discount)}</span></div>` : ''}
  <div class="tot"><span>Tổng cộng</span><b>${money(o.total)}</b></div></div>`;
const seatChips = o => `<div class="chips">${o.tickets.map(t => `<span class="chip ${t.type === 'VIP' ? 'vip' : ''}">${esc(t.seat)}</span>`).join('')}</div>`;
const countdown = o => `<span class="countdown" data-until="${o.holdUntil}">--:--</span>`;

function payArea(o) {
  const m = P.method;
  if (m === 'BANK' || m === 'WALLET') {
    return `<div class="paybox"><div class="qrbox">${qrSvg('CINEQ|' + o.code + '|' + o.total + '|' + m)}</div>
      <div><b>${METHODS[m][0]}</b><div class="muted">Nội dung: <span class="mono">${esc(o.code)}</span></div><div style="font:800 26px var(--head);margin-top:4px">${money(o.total)}</div></div></div>`;
  }
  if (m === 'CASH') {
    const chips = [o.total, 100000, 200000, 500000].filter((v, i, a) => v >= o.total && a.indexOf(v) === i);
    const c = P.cash, left = c - o.total;
    return `<div class="paybox" style="display:block"><div class="field"><label class="lab" for="cash">Tiền khách đưa</label><input class="in" id="cash" data-in="cash" inputmode="numeric" autocomplete="off" value="${c ? c.toLocaleString('vi-VN') : ''}"></div>
      <div class="cashchips">${chips.map(v => `<button class="chip" data-act="cashPick" data-v="${v}">${money(v)}</button>`).join('')}</div>
      <div class="change ${c && left < 0 ? 'err' : ''}" id="change">${!c ? '' : left < 0 ? 'Còn thiếu ' + money(-left) : 'Tiền thối: ' + money(left)}</div></div>`;
  }
  if (m === 'CARD') return `<div class="paybox"><div><b>${METHODS.CARD[0]}</b><div class="muted">Bấm thanh toán để chuyển sang cổng thẻ.</div></div></div>`;
  return '';
}

async function vPay(id) {
  if (!S.user) { draw('', `<section class="sec wrap"><div class="empty"><p>Đăng nhập để thanh toán đơn hàng.</p><p style="margin-top:14px"><button class="btn primary" data-act="login">Đăng nhập</button></p></div></section>`); return; }
  const o = (await loadOrders()).find(x => x.id == id);
  if (!o) return notFound();
  if (o.status === 'PAID') { location.hash = '#/ticket/' + o.id; return; }
  if (o.status !== 'HOLD') {
    draw('orders', `<section class="sec wrap"><div class="alert bad"><span>Đơn ${esc(o.code)} ${o.status === 'EXPIRED' ? 'đã hết thời gian giữ ghế' : 'đã bị hủy'}. Ghế đã được nhả.</span><a class="btn" href="#/movie/${(S.cat.movies.find(m => m.title === o.movie) || {}).id || ''}">Đặt lại</a></div></section>`);
    return;
  }
  S.curOrder = o;
  if (!P.method) P.method = isAdmin() ? 'CASH' : 'BANK';
  const keys = isAdmin() ? ['CASH', 'BANK', 'WALLET', 'CARD'] : ['BANK', 'WALLET', 'CARD'];
  draw('orders', `${crumbs([['#/orders', isAdmin() ? 'Đơn hàng' : 'Vé của tôi'], [null, 'Thanh toán ' + o.code]])}
    <section class="wrap" style="padding-block:6px 10px"><div id="bsteps">${steps(3)}</div>
    <div class="book"><div><div class="card"><div class="sec-h" style="margin-bottom:14px"><h3 style="font-size:24px">Thanh toán</h3>
        <div style="text-align:right"><div class="lab">Giữ ghế còn</div>${countdown(o)}</div></div>
      ${o.fromQueue ? '<div class="alert ok" style="margin-bottom:14px"><span>Ghế này được giữ cho bạn từ danh sách chờ.</span></div>' : ''}
      <div class="methods">${keys.map(k => `<label class="method"><input type="radio" name="pm" value="${k}" data-in="method" ${P.method === k ? 'checked' : ''}><span><b>${METHODS[k][0]}</b><small>${METHODS[k][1]}</small></span></label>`).join('')}</div>
      <div id="payarea">${payArea(o)}</div><div class="err" id="pay-err" style="margin-top:8px"></div></div></div>
    <aside class="side card"><div class="sumhead">${miniPoster(o.movie)}<div><h3>${esc(o.movie)}</h3><p>${fmtT(o.start)} · ${fmtD(o.start)} · ${esc(o.room)}</p><p class="mono">${esc(o.code)}</p></div></div>
      ${seatChips(o)}${orderLines(o)}
      <button class="btn primary block lg" data-act="pay" data-id="${o.id}" id="paybtn">Thanh toán ${money(o.total)}</button>
      <button class="btn block warn" style="margin-top:8px" data-act="cancelOrder" data-id="${o.id}" data-back="1">Hủy giữ ghế</button></aside></div></section>`);
  S.refresh = async () => { const cur = (await loadOrders()).find(x => x.id == id); if (!cur || cur.status !== 'HOLD') route(); };
}

async function doPay(id) {
  const btn = $('#paybtn'), err = $('#pay-err'); err.textContent = '';
  if (P.method === 'CASH' && !P.cash) { err.textContent = 'Nhập số tiền khách đưa.'; return; }
  btn.disabled = true; btn.textContent = 'Đang xử lý…';
  try {
    if (P.method !== 'CASH') await sleep(900);
    const d = await api('/orders/pay', { orderId: id, method: P.method, cash: P.cash });
    S.change = d.change || 0; P = { method: '', cash: 0 };
    location.hash = '#/ticket/' + id;
  } catch (e) { err.textContent = e.message; btn.disabled = false; btn.textContent = 'Thanh toán'; if (e.status === 400 && /hết thời gian|hủy/.test(e.message)) route(); }
}

async function vTicket(id) {
  if (!S.user) return route401();
  const o = (await loadOrders()).find(x => x.id == id);
  if (!o) return notFound();
  if (o.status === 'HOLD') { location.hash = '#/pay/' + id; return; }
  const used = !!o.checkedInAt, void_ = o.status !== 'PAID';
  const m = S.cat.movies.find(x => x.title === o.movie);
  draw('orders', `${crumbs([['#/orders', isAdmin() ? 'Đơn hàng' : 'Vé của tôi'], [null, o.code]])}<section class="wrap" style="padding-block:10px 20px">
    ${S.change ? `<div class="alert ok" style="max-width:760px;margin:0 auto 16px"><span>Đã thu tiền. Tiền thối khách: <b>${money(S.change)}</b></span></div>` : ''}
    <div class="etix ${void_ ? 'void' : ''}"><div class="main">
      <div class="row"><span class="badge ${STATUS[o.status][1]}">${STATUS[o.status][0]}</span>${used ? `<span class="badge">Đã soát vé lúc ${fmtT(o.checkedInAt)}</span>` : ''}${m ? `<span class="badge gold">${esc(m.rating)}</span>` : ''}</div>
      <h2>${esc(o.movie)}</h2>
      <dl><div><dt>Giờ chiếu</dt><dd>${fmtT(o.start)} · ${fmtD(o.start)}</dd></div><div><dt>Phòng</dt><dd>${esc(o.room)}</dd></div><div><dt>Khách</dt><dd>${esc(o.name)}</dd></div>
        <div><dt>Thanh toán</dt><dd>${esc((METHODS[o.method] || [o.method])[0])}</dd></div>${o.payRef && o.payRef !== 'CASH' ? `<div><dt>Mã giao dịch</dt><dd class="mono" style="font-size:14px">${esc(o.payRef)}</dd></div>` : ''}
        <div><dt>Tổng tiền</dt><dd>${money(o.total)}</dd></div></dl>
      <div><div class="lab" style="margin-bottom:6px">Ghế</div><div class="seatbig">${o.tickets.map(t => `<span class="${t.type === 'VIP' ? 'vip' : ''}">${esc(t.seat)}</span>`).join('')}</div></div>
      ${o.combos.length ? `<div><div class="lab" style="margin-bottom:4px">Bắp nước</div><div>${o.combos.map(c => esc(c.name) + ' × ' + c.qty).join(' · ')}</div></div>` : ''}
    </div><div class="stub"><div class="qrbox">${qrSvg(o.code)}</div><div class="code">${esc(o.code)}</div><div class="muted" style="font-size:12px">Đưa mã này tại cửa soát vé</div></div></div>
    <div class="row" style="justify-content:center;margin-top:18px"><button class="btn primary" data-act="print">In vé</button><a class="btn" href="#/orders">${isAdmin() ? 'Đơn hàng' : 'Vé của tôi'}</a></div></section>`);
  S.change = 0;
}
function route401() { draw('', `<section class="sec wrap"><div class="empty"><p>Đăng nhập để xem nội dung này.</p><p style="margin-top:14px"><button class="btn primary" data-act="login">Đăng nhập</button></p></div></section>`); }

function orderCard(o) {
  const st = STATUS[o.status], hold = o.status === 'HOLD';
  return `<div class="card ord">${miniPoster(o.movie)}
    <div style="min-width:0"><div class="row" style="margin-bottom:4px"><span class="badge ${st[1]}">${st[0]}</span>${o.checkedInAt ? '<span class="badge">Đã soát vé</span>' : ''}${o.fromQueue ? '<span class="badge gold">Từ danh sách chờ</span>' : ''}<span class="mono muted">${esc(o.code)}</span></div>
      <h3>${esc(o.movie)}</h3><div class="muted" style="font-size:14px;margin:2px 0 8px">${fmtT(o.start)} · ${fmtD(o.start)} · ${esc(o.room)} · ${esc(o.name)}${o.owner ? ' (' + esc(o.owner) + ')' : ''}</div>
      ${seatChips(o)}
      ${hold ? `<div style="margin-top:8px" class="muted">Giữ ghế còn <b data-until="${o.holdUntil}" class="mono">--:--</b></div>` : ''}
      ${o.status === 'CANCELLED' && o.refund ? `<div style="margin-top:8px" class="muted">Đã hoàn ${money(o.refund)}</div>` : ''}</div>
    <div style="text-align:right"><div style="font:800 24px var(--head)">${money(o.total)}</div>
      <div class="acts" style="margin-top:8px">${hold ? `<a class="btn sm primary" href="#/pay/${o.id}">Thanh toán</a>` : ''}${o.status === 'PAID' ? `<a class="btn sm" href="#/ticket/${o.id}">Xem vé</a>` : ''}
        ${o.canCancel ? `<button class="btn sm warn" data-act="cancelOrder" data-id="${o.id}">${hold ? 'Hủy giữ ghế' : 'Hủy vé'}</button>` : ''}</div></div></div>`;
}
async function vOrders() {
  if (!S.user) return route401();
  const load = async () => {
    let list = await loadOrders();
    const total = list.length;
    if (S.filter === 'HOLD') list = list.filter(o => o.status === 'HOLD');
    else if (S.filter === 'PAID') list = list.filter(o => o.status === 'PAID');
    else if (S.filter === 'VOID') list = list.filter(o => o.status === 'CANCELLED' || o.status === 'EXPIRED');
    const q = S.q.trim().toLowerCase();
    if (q) list = list.filter(o => (o.code + ' ' + o.name + ' ' + o.movie + ' ' + o.owner).toLowerCase().includes(q));
    return { list, total };
  };
  const { list, total } = await load();
  const f = [['ALL', 'Tất cả'], ['HOLD', 'Chờ thanh toán'], ['PAID', 'Đã thanh toán'], ['VOID', 'Đã hủy']];
  draw('orders', `<section class="sec wrap"><div class="sec-h"><h2>${isAdmin() ? 'Đơn hàng' : 'Vé của tôi'}</h2>${isAdmin() ? '<button class="btn sm" data-act="csv">Xuất CSV</button>' : ''}</div>
    <div class="filters"><div class="seg">${f.map(([k, l]) => `<button data-act="filter" data-v="${k}" aria-pressed="${S.filter === k}">${l}</button>`).join('')}</div>
      <div class="grow"><input class="in" id="oq" data-in="oq" type="search" placeholder="Tìm theo mã đơn, tên khách, phim" value="${esc(S.q)}"></div></div>
    <div class="orders" id="olist">${list.length ? list.map(orderCard).join('') : `<div class="empty">${total ? 'Không có đơn khớp bộ lọc.' : 'Chưa có đơn nào. <a href="#/">Chọn phim</a> để đặt vé.'}</div>`}</div></section>`);
  S.refresh = async () => { if (document.activeElement && document.activeElement.id === 'oq') return; const r = await load(); const el = $('#olist'); if (el) { el.innerHTML = r.list.length ? r.list.map(orderCard).join('') : '<div class="empty">Không có đơn khớp bộ lọc.</div>'; tick(); } };
}

function cancelDlg(id, back) {
  loadOrders().then(list => {
    const o = list.find(x => x.id == id); if (!o) return;
    const r = S.cat.rules, hold = o.status === 'HOLD';
    confirmDlg(hold ? 'Hủy giữ ghế?' : 'Hủy vé ' + o.code + '?',
      hold ? `<p>Các ghế <b>${o.tickets.map(t => esc(t.seat)).join(', ')}</b> sẽ được nhả ngay.</p>`
           : `<p>Ghế <b>${o.tickets.map(t => esc(t.seat)).join(', ')}</b> sẽ được trả lại. Hoàn tiền <b>${money(o.refundPreview)}</b> (${r.refundPercent}% giá trị đơn).</p><p class="muted" style="margin-top:8px">Khách chỉ tự hủy được trước giờ chiếu ít nhất ${r.cancelBeforeMin / 60} tiếng.</p>`,
      hold ? 'Hủy giữ ghế' : 'Hủy vé', async () => {
        const d = await api('/orders/cancel', { orderId: id });
        toast(hold ? 'Đã hủy giữ ghế.' : 'Đã hủy vé. Hoàn ' + money(d.refund) + '.' + (d.granted.length ? ' Ghế được chuyển cho ' + d.granted.map(g => g.name).join(', ') + ' trong danh sách chờ.' : ''));
        if (back) location.hash = '#/orders'; else await route(true);
      });
  }).catch(e => toast(e.message, 'bad'));
}

/* ---------- tin tức ---------- */
async function vNews() {
  const d = await api('/news' + (S.newsCat ? '?cat=' + encodeURIComponent(S.newsCat) : ''));
  window.NEWS = d.items; window.NCATS = d.categories;
  draw('news', `<section class="sec wrap"><div class="sec-h"><h2>Tin tức</h2>${isAdmin() ? '<button class="btn primary sm" data-act="newsNew">Đăng tin</button>' : ''}</div>
    <div class="filters"><div class="seg"><button data-act="newsCat" data-v="" aria-pressed="${S.newsCat === ''}">Tất cả</button>${d.categories.map(c => `<button data-act="newsCat" data-v="${esc(c)}" aria-pressed="${S.newsCat === c}">${esc(c)}</button>`).join('')}</div></div>
    <div class="news">${d.items.length ? d.items.map(n => {
      const m = n.movie && S.cat.movies.find(x => x.title === n.movie);
      return `<article class="card post ${n.pinned ? 'pin' : ''}"><div class="row meta">${n.pinned ? '<span class="badge gold">Ghim</span>' : ''}<span class="badge">${esc(n.category)}</span><span class="muted mono" style="font-size:12px">${fmtD(n.ts)}/${new Date(n.ts * 1000).getFullYear()}</span></div>
        <h3>${esc(n.title)}</h3><p class="body">${esc(n.body)}</p>
        ${n.movie ? `<div class="alert" style="margin-top:12px"><span>Phim: <b>${esc(n.movie)}</b></span>${m ? `<a class="btn sm primary" href="#/movie/${m.id}">Mua vé</a>` : ''}</div>` : ''}
        <div class="muted" style="font-size:13px;margin-top:10px">${esc(n.author)}</div>
        ${isAdmin() ? `<div class="row" style="margin-top:10px"><button class="btn sm" data-act="newsEdit" data-id="${n.id}">Sửa</button><button class="btn sm warn" data-act="newsDel" data-id="${n.id}">Xóa</button></div>` : ''}</article>`;
    }).join('') : '<div class="empty">Chưa có tin nào.</div>'}</section>`);
}
function newsModal(id) {
  const n = id ? NEWS.find(x => x.id == id) : null;
  const v = n || { category: NCATS[0], title: '', body: '', movie: '', pinned: false };
  const m = modal(`<div class="dlg-h">${n ? 'Sửa tin' : 'Đăng tin'}</div><form id="nf">
    <div class="fgrid"><div class="field"><label class="lab" for="n-cat">Chuyên mục</label><select class="in" id="n-cat">${NCATS.map(c => `<option ${c === v.category ? 'selected' : ''}>${esc(c)}</option>`).join('')}</select></div>
    <div class="field"><label class="lab" for="n-movie">Phim liên quan</label><select class="in" id="n-movie"><option value="">Không</option>${S.cat.movies.map(x => `<option ${x.title === v.movie ? 'selected' : ''}>${esc(x.title)}</option>`).join('')}</select></div></div>
    <div class="field"><label class="lab" for="n-title">Tiêu đề</label><input class="in" id="n-title" maxlength="120" value="${esc(v.title)}"></div>
    <div class="field"><label class="lab" for="n-body">Nội dung</label><textarea class="in" id="n-body">${esc(v.body)}</textarea></div>
    <label class="chk field"><input type="checkbox" id="n-pin" ${v.pinned ? 'checked' : ''}><span>Ghim lên đầu</span></label>
    <div class="err" id="n-err"></div><div class="dlg-act"><button type="button" class="btn" data-close>Hủy</button><button class="btn primary" type="submit">${n ? 'Lưu' : 'Đăng'}</button></div></form>`, true);
  m.root.querySelector('#nf').onsubmit = async e => {
    e.preventDefault();
    const g = i => m.root.querySelector(i);
    const body = { category: g('#n-cat').value, title: g('#n-title').value.trim(), body: g('#n-body').value.trim(), movie: g('#n-movie').value, pinned: g('#n-pin').checked };
    try { await api(n ? '/news/update' : '/news/create', n ? { ...body, id: n.id } : body); m.close(); toast('Đã lưu tin.'); route(true); }
    catch (er) { g('#n-err').textContent = er.message; }
  };
}

/* ---------- quản trị ---------- */
function needAdmin() { if (isAdmin()) return false; draw('', `<section class="sec wrap"><div class="empty">Chức năng này chỉ dành cho quản trị viên.</div></section>`); return true; }

async function vCheckin() {
  if (needAdmin()) return;
  draw('checkin', `<section class="sec wrap"><div class="sec-h"><h2>Soát vé</h2></div><div class="scan"><form id="cf" class="card" style="display:grid;gap:12px">
    <label class="lab" for="code">Mã vé</label><input class="in" id="code" placeholder="CQ000123" autocomplete="off" autofocus>
    <button class="btn primary lg" type="submit">Soát vé</button></form><div id="cres"></div></div></section>`);
  const f = $('#code'); f.focus();
  $('#cf').onsubmit = async e => {
    e.preventDefault();
    const out = $('#cres');
    try {
      const d = await api('/checkin', { code: f.value });
      const o = d.order;
      out.innerHTML = `<div class="card" style="border-color:var(--ok)"><div class="row" style="margin-bottom:8px"><span class="badge ok">Hợp lệ</span><span class="mono">${esc(o.code)}</span></div>
        <h3 style="font:700 20px var(--body);text-transform:none">${esc(o.movie)}</h3><div class="muted">${fmtT(o.start)} · ${fmtD(o.start)} · ${esc(o.room)} · ${esc(o.name)}</div><div style="margin-top:10px">${seatChips(o)}</div></div>`;
      f.value = ''; f.focus();
    } catch (er) { out.innerHTML = `<div class="alert bad"><span>${esc(er.message)}</span></div>`; f.select(); }
  };
}

async function vReports() {
  if (needAdmin()) return;
  const [s, sh] = await Promise.all([api('/stats'), api('/shows')]);
  const byId = Object.fromEntries(sh.items.map(x => [x.id, x]));
  const occ = s.shows.filter(x => byId[x.id] && x.total).sort((a, b) => byId[a.id].start - byId[b.id].start);
  const maxRev = Math.max(1, ...s.movies.map(m => m.revenue));
  draw('reports', `<section class="sec wrap"><div class="sec-h"><h2>Báo cáo</h2><button class="btn sm" data-act="resetSales">Đặt lại dữ liệu bán vé</button></div>
    <div class="kpis"><div class="kpi"><b>${money(s.net)}</b><span>Doanh thu thuần</span></div><div class="kpi"><b>${money(s.gross)}</b><span>Doanh thu gộp</span></div><div class="kpi"><b>${money(s.refunds)}</b><span>Đã hoàn tiền</span></div>
      <div class="kpi"><b>${s.ticketsSold}</b><span>Vé đã bán</span></div><div class="kpi"><b>${s.checkedIn}</b><span>Đơn đã soát</span></div><div class="kpi"><b>${s.ordersHold}</b><span>Đơn đang giữ ghế</span></div>
      <div class="kpi"><b>${s.ordersCancelled + s.ordersExpired}</b><span>Đơn hủy / hết hạn</span></div><div class="kpi"><b>${s.waiting}</b><span>Đang chờ ghế</span></div></div>
    <div class="two"><div class="card"><h3 style="font-size:22px;margin-bottom:10px">Doanh thu theo phim</h3><div class="tscroll"><table class="tbl"><tr><th>Phim</th><th class="r">Vé</th><th class="r">Doanh thu</th><th></th></tr>
      ${s.movies.filter(m => m.revenue || m.tickets).map(m => `<tr><td>${esc(m.title)}</td><td class="r">${m.tickets}</td><td class="r">${money(m.revenue)}</td><td><div class="bar"><i style="width:${Math.round(m.revenue * 100 / maxRev)}%"></i></div></td></tr>`).join('') || '<tr><td colspan="4" class="muted">Chưa có doanh thu.</td></tr>'}</table></div></div>
    <div class="card"><h3 style="font-size:22px;margin-bottom:10px">Bắp nước</h3><div class="tscroll"><table class="tbl"><tr><th>Món</th><th class="r">Số lượng</th><th class="r">Doanh thu</th></tr>
      ${s.combos.map(c => `<tr><td>${esc(c.name)}</td><td class="r">${c.qty}</td><td class="r">${money(c.revenue)}</td></tr>`).join('') || '<tr><td colspan="3" class="muted">Chưa bán.</td></tr>'}</table></div></div></div>
    <div class="card" style="margin-top:16px"><h3 style="font-size:22px;margin-bottom:10px">Lấp đầy theo suất</h3><div class="tscroll"><table class="tbl"><tr><th>Suất</th><th>Phòng</th><th class="r">Ghế bán</th><th style="min-width:140px"></th></tr>
      ${occ.map(x => { const sw = byId[x.id], p = Math.round(x.taken * 100 / x.total); return `<tr><td>${fmtD(sw.start)} ${fmtT(sw.start)} · ${esc(sw.movie)}</td><td>${esc(sw.room)}</td><td class="r">${x.taken}/${x.total} (${p}%)</td><td><div class="bar"><i style="width:${p}%"></i></div></td></tr>`; }).join('')}</table></div></div></section>`);
}

async function vCatalog() {
  if (needAdmin()) return;
  const sh = (await api('/shows')).items;
  draw('catalog', `<section class="sec wrap"><div class="sec-h"><h2>Phim</h2><button class="btn primary sm" data-act="movieNew">Thêm phim</button></div>
    <div class="card"><div class="tscroll"><table class="tbl"><tr><th>Tên phim</th><th>Thể loại</th><th class="r">Phút</th><th>Nhãn</th><th>Trạng thái</th><th class="r">Suất</th><th></th></tr>
    ${S.cat.movies.map(m => `<tr><td><b>${esc(m.title)}</b></td><td>${esc(m.genre)}</td><td class="r">${m.minutes}</td><td>${esc(m.rating)}</td><td>${m.status === 'NOW' ? 'Đang chiếu' : 'Sắp chiếu'}</td><td class="r">${m.shows}</td>
      <td class="r"><button class="btn sm" data-act="movieEdit" data-id="${m.id}">Sửa / Đổi ảnh</button> <button class="btn sm warn" data-act="movieDel" data-id="${m.id}">Xóa</button></td></tr>`).join('')}</table></div></div>
    <div class="sec-h" style="margin-top:32px"><h2>Suất chiếu</h2><button class="btn primary sm" data-act="showNew">Thêm suất</button></div>
    <div class="card"><div class="tscroll"><table class="tbl"><tr><th>Giờ</th><th>Phim</th><th>Phòng</th><th class="r">Giá thường</th><th class="r">Giá VIP</th><th class="r">Trống</th><th></th></tr>
    ${sh.map(s => `<tr><td class="mono">${fmtD(s.start)} ${fmtT(s.start)}</td><td>${esc(s.movie)}</td><td>${esc(s.room)}</td><td class="r">${money(s.priceNormal)}</td><td class="r">${money(s.priceVip)}</td><td class="r">${s.freeNormal + s.freeVip}</td>
      <td class="r"><button class="btn sm warn" data-act="showDel" data-id="${s.id}">Xóa</button></td></tr>`).join('')}</table></div></div></section>`);
  window.SHOWS = sh;
}
function movieModal(id) {
  const m0 = id ? S.cat.movies.find(x => x.id == id) : null, v = m0 || { title: '', genre: '', minutes: 100, rating: 'P', status: 'NOW', synopsis: '' };
  const curImg = m0 ? getPosterUrl(m0.title) : '';
  const m = modal(`<div class="dlg-h">${m0 ? 'Sửa phim' : 'Thêm phim'}</div><form id="mf">
    <div class="field"><label class="lab" for="m-t">Tên phim</label><input class="in" id="m-t" maxlength="80" value="${esc(v.title)}"></div>
    <div class="field"><label class="lab" for="m-img">Link ảnh Poster (Dán địa chỉ hình ảnh từ Google vào đây)</label><input class="in" id="m-img" placeholder="https://..." value="${esc(curImg)}"></div>
    <div class="fgrid"><div class="field"><label class="lab" for="m-g">Thể loại</label><input class="in" id="m-g" value="${esc(v.genre)}"></div><div class="field"><label class="lab" for="m-m">Thời lượng (phút)</label><input class="in" id="m-m" type="number" min="30" max="300" value="${v.minutes}"></div>
      <div class="field"><label class="lab" for="m-r">Nhãn tuổi</label><select class="in" id="m-r">${['P', 'T13', 'T16', 'T18'].map(r => `<option ${r === v.rating ? 'selected' : ''}>${r}</option>`).join('')}</select></div>
      <div class="field"><label class="lab" for="m-s">Trạng thái</label><select class="in" id="m-s"><option value="NOW" ${v.status === 'NOW' ? 'selected' : ''}>Đang chiếu</option><option value="SOON" ${v.status === 'SOON' ? 'selected' : ''}>Sắp chiếu</option></select></div></div>
    <div class="field"><label class="lab" for="m-y">Nội dung</label><textarea class="in" id="m-y">${esc(v.synopsis)}</textarea></div>
    <div class="err" id="m-err"></div><div class="dlg-act"><button type="button" class="btn" data-close>Hủy</button><button class="btn primary" type="submit">Lưu</button></div></form>`, true);
  m.root.querySelector('#mf').onsubmit = async e => {
    e.preventDefault(); const g = i => m.root.querySelector(i).value;
    const newTitle = g('#m-t').trim();
    const imgUrl = g('#m-img').trim();
    try {
      await api('/movies/save', { id: id ? Number(id) : 0, title: newTitle, genre: g('#m-g').trim(), minutes: Number(g('#m-m')), rating: g('#m-r'), status: g('#m-s'), synopsis: g('#m-y').trim() });
      if (newTitle) saveCustomPoster(newTitle, imgUrl);
      m.close(); toast('Đã lưu phim.'); route(true);
    } catch (er) { m.root.querySelector('#m-err').textContent = er.message; }
  };
}
function showModal() {
  const t = new Date(Date.now() + 3600e3); t.setMinutes(0, 0, 0);
  const iso = `${t.getFullYear()}-${d2(t.getMonth() + 1)}-${d2(t.getDate())}T${d2(t.getHours())}:00`;
  const m = modal(`<div class="dlg-h">Thêm suất chiếu</div><form id="sf">
    <div class="field"><label class="lab" for="s-m">Phim</label><select class="in" id="s-m">${S.cat.movies.map(x => `<option value="${x.id}">${esc(x.title)}</option>`).join('')}</select></div>
    <div class="fgrid"><div class="field"><label class="lab" for="s-r">Phòng</label><input class="in" id="s-r" value="Phòng 1"></div><div class="field"><label class="lab" for="s-t">Giờ chiếu</label><input class="in" id="s-t" type="datetime-local" value="${iso}"></div>
      <div class="field"><label class="lab" for="s-rows">Số hàng</label><input class="in" id="s-rows" type="number" min="1" max="15" value="5"></div><div class="field"><label class="lab" for="s-cols">Số ghế mỗi hàng</label><input class="in" id="s-cols" type="number" min="2" max="20" value="8"></div>
      <div class="field"><label class="lab" for="s-vip">Số hàng VIP</label><input class="in" id="s-vip" type="number" min="0" max="15" value="1"></div><div></div>
      <div class="field"><label class="lab" for="s-pn">Giá thường (đ)</label><input class="in" id="s-pn" type="number" min="1000" step="1000" value="80000"></div><div class="field"><label class="lab" for="s-pv">Giá VIP (đ)</label><input class="in" id="s-pv" type="number" min="1000" step="1000" value="120000"></div></div>
    <div class="err" id="s-err"></div><div class="dlg-act"><button type="button" class="btn" data-close>Hủy</button><button class="btn primary" type="submit">Thêm</button></div></form>`, true);
  m.root.querySelector('#sf').onsubmit = async e => {
    e.preventDefault(); const g = i => m.root.querySelector(i).value;
    try { await api('/showtimes/save', { movieId: Number(g('#s-m')), room: g('#s-r').trim(), start: Math.floor(new Date(g('#s-t')).getTime() / 1000) || 0, rows: Number(g('#s-rows')), cols: Number(g('#s-cols')), vipRows: Number(g('#s-vip')), priceNormal: Number(g('#s-pn')), priceVip: Number(g('#s-pv')) }); m.close(); toast('Đã thêm suất chiếu.'); route(true); }
    catch (er) { m.root.querySelector('#s-err').textContent = er.message; }
  };
}

/* ---------- đồng hồ đếm ngược giữ ghế ---------- */
let tickBusy = false;
function tick() {
  document.querySelectorAll('[data-until]').forEach(el => {
    const left = Number(el.dataset.until) - nowSec();
    if (left <= 0) {
      el.textContent = '00:00';
      if (!tickBusy && !el.dataset.done) { el.dataset.done = '1'; tickBusy = true; route(true).finally(() => { tickBusy = false; }); }
      return;
    }
    el.textContent = d2(Math.floor(left / 60)) + ':' + d2(left % 60);
    el.classList.toggle('low', left < 60);
  });
}
setInterval(tick, 1000);

/* ---------- hành động ---------- */
const A = {
  theme() { const cur = document.documentElement.dataset.theme || (matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light'); const nx = cur === 'dark' ? 'light' : 'dark'; document.documentElement.dataset.theme = nx; store.set('cq_theme', nx); },
  login() { authModal('login'); },
  register() { authModal('register'); },
  async logout() { try { await api('/logout', {}); } catch { /* bỏ qua */ } S.token = ''; S.user = null; B = null; S.grants.clear(); store.set('cq_token', ''); toast('Đã đăng xuất.'); if (/^#\/(orders|pay|ticket|admin)/.test(location.hash)) location.hash = '#/'; else route(true); },
  homeTab(el) { S.homeTab = el.dataset.v; route(true); },
  date(el) { S.date = el.dataset.v; route(true); },
  seat(el) {
    const r = Number(el.dataset.r), c = Number(el.dataset.c), i = B.sel.findIndex(p => p.row === r && p.col === c);
    if (i >= 0) B.sel.splice(i, 1);
    else { if (B.sel.length >= S.cat.rules.maxSeats) return toast('Tối đa ' + S.cat.rules.maxSeats + ' ghế mỗi đơn.', 'bad'); B.sel.push({ row: r, col: c }); }
    refreshQuote().then(bookPaint);
    bookPaint();
  },
  cnt(el) { B.count = Math.max(1, Math.min(S.cat.rules.maxSeats, B.count + Number(el.dataset.d))); bookPaint(); },
  qtype(el) { B.type = el.dataset.v; bookPaint(); },
  async auto() {
    try {
      const d = await api('/quote', { showId: B.showId, count: B.count, type: B.type });
      B.sel = d.seats.map(s => ({ row: s.row, col: s.col }));
      await refreshQuote(); bookPaint();
    } catch (e) { toast(e.message, 'bad'); }
  },
  clearSel() { B.sel = []; B.quote = null; bookPaint(); },
  step1() { B.step = 1; bookPaint(); window.scrollTo(0, 0); },
  step2() { if (!B.sel.length) return; B.step = 2; bookPaint(); window.scrollTo(0, 0); },
  combo(el) { const id = el.dataset.id, n = Math.max(0, Math.min(10, (B.combos[id] || 0) + Number(el.dataset.d))); B.combos[id] = n; refreshQuote().then(bookPaint); bookPaint(); },
  async promo() {
    const code = (B.promoText || '').trim(); if (!code) { B.promoErr = 'Nhập mã giảm giá.'; return bookPaint(); }
    try {
      const combos = {}; Object.entries(B.combos).forEach(([k, v]) => { if (v > 0) combos[k] = v; });
      const q = await api('/quote', { showId: B.showId, seats: B.sel.map(p => ({ row: p.row, col: p.col })), combos, promo: code });
      B.promoOk = code.toUpperCase(); B.promoErr = ''; B.quote = q; toast('Đã áp dụng mã ' + B.promoOk + '.');
    } catch (e) { B.promoErr = e.message; }
    bookPaint();
  },
  promoPick(el) { B.promoText = el.dataset.v; B.promoErr = ''; bookPaint(); },
  async promoClear() { B.promoOk = ''; B.promoText = ''; await refreshQuote(); bookPaint(); },
  hold() { requireLogin(async () => { if (!B) return route(); await holdOrder(); }); },
  joinQ(el) {
    requireLogin(async () => {
      try {
        const d = await api('/queue/join', { showId: B.showId, type: el.dataset.t, name: isAdmin() ? (B.name.trim() || 'Khách lẻ') : S.user.fullName });
        toast('Bạn đang ở vị trí ' + d.position + ' trong danh sách chờ.'); await route(true);
      } catch (e) { toast(e.message, 'bad'); }
    });
  },
  async leaveQ(el) { try { await api('/queue/leave', { showId: B.showId, customerId: Number(el.dataset.id) }); toast('Đã rời danh sách chờ.'); await route(true); } catch (e) { toast(e.message, 'bad'); } },
  pay(el) { doPay(Number(el.dataset.id)); },
  cashPick(el) { P.cash = Number(el.dataset.v); const i = $('#cash'); if (i) { i.value = P.cash.toLocaleString('vi-VN'); i.dispatchEvent(new Event('input', { bubbles: true })); } },
  cancelOrder(el) { cancelDlg(Number(el.dataset.id), !!el.dataset.back); },
  print() { window.print(); },
  filter(el) { S.filter = el.dataset.v; route(true); },
  async csv() {
    const list = await loadOrders();
    const q = v => '"' + String(v ?? '').replace(/"/g, '""') + '"';
    const rows = [['Mã đơn', 'Phim', 'Giờ chiếu', 'Phòng', 'Khách', 'Tài khoản', 'Trạng thái', 'Thanh toán', 'Tổng tiền', 'Hoàn tiền']];
    list.forEach(o => rows.push([o.code, o.movie, fmtD(o.start) + ' ' + fmtT(o.start), o.room, o.name, o.owner, STATUS[o.status][0], (METHODS[o.method] || [''])[0], o.total, o.refund]));
    const a = document.createElement('a');
    a.href = URL.createObjectURL(new Blob(['\uFEFF' + rows.map(r => r.map(q).join(',')).join('\r\n')], { type: 'text/csv;charset=utf-8' }));
    a.download = 'don-hang.csv'; a.click(); setTimeout(() => URL.revokeObjectURL(a.href), 1000);
  },
  newsCat(el) { S.newsCat = el.dataset.v; route(true); },
  newsNew() { newsModal(0); },
  newsEdit(el) { newsModal(el.dataset.id); },
  newsDel(el) { confirmDlg('Xóa tin này?', '<p>Tin sẽ bị xóa vĩnh viễn.</p>', 'Xóa', async () => { await api('/news/delete', { id: Number(el.dataset.id) }); toast('Đã xóa tin.'); route(true); }); },
  movieNew() { movieModal(0); },
  movieEdit(el) { movieModal(el.dataset.id); },
  movieDel(el) { confirmDlg('Xóa phim?', '<p>Chỉ xóa được phim chưa có suất chiếu.</p>', 'Xóa', async () => { await api('/movies/delete', { id: Number(el.dataset.id) }); toast('Đã xóa phim.'); route(true); }); },
  showNew() { showModal(); },
  showDel(el) { confirmDlg('Xóa suất chiếu?', '<p>Chỉ xóa được suất chưa có đơn hàng đang hiệu lực.</p>', 'Xóa', async () => { await api('/showtimes/delete', { id: Number(el.dataset.id) }); toast('Đã xóa suất chiếu.'); route(true); }); },
  resetSales() { confirmDlg('Đặt lại dữ liệu bán vé?', '<p>Toàn bộ đơn hàng và danh sách chờ sẽ bị xóa. Phim, suất chiếu và tin tức được giữ nguyên.</p>', 'Đặt lại', async () => { await api('/reset', {}); B = null; toast('Đã đặt lại.'); route(true); }); }
};
document.addEventListener('click', e => {
  const el = e.target.closest('[data-act]'); if (!el || el.disabled) return;
  const fn = A[el.dataset.act]; if (!fn) return;
  Promise.resolve().then(() => fn(el, e)).catch(er => toast(er.message, 'bad'));
});
document.addEventListener('input', e => {
  const k = e.target.dataset && e.target.dataset.in; if (!k) return;
  if (k === 'promo') B.promoText = e.target.value;
  else if (k === 'cname') B.name = e.target.value;
  else if (k === 'age') B.age = e.target.checked;
  else if (k === 'cash') {
    P.cash = parseInt(e.target.value.replace(/\D/g, ''), 10) || 0;
    const o = S.curOrder, ch = $('#change');
    if (o && ch) { const left = P.cash - o.total; ch.className = 'change' + (P.cash && left < 0 ? ' err' : ''); ch.textContent = !P.cash ? '' : left < 0 ? 'Còn thiếu ' + money(-left) : 'Tiền thối: ' + money(left); }
  }
  else if (k === 'oq') { S.q = e.target.value; clearTimeout(A._qt); A._qt = setTimeout(() => S.refresh && S.refresh(), 250); }
});
document.addEventListener('change', e => {
  if (e.target.dataset && e.target.dataset.in === 'method') {
    P.method = e.target.value; const o = S.curOrder; const pa = $('#payarea'); if (o && pa) pa.innerHTML = payArea(o);
  }
});
document.addEventListener('keydown', e => { if (e.key === 'Enter' && e.target.id === 'promo') { e.preventDefault(); A.promo(); } });

/* ---------- định tuyến ---------- */
async function route(keep) {
  const [a, b, c] = location.hash.replace(/^#\/?/, '').split('/');
  S.refresh = null;
  try {
    S.cat = await api('/catalog'); S.offset = S.cat.now - Math.floor(Date.now() / 1000);
    if (S.token && !S.user) { S.user = (await api('/me')).user; if (!S.user) { S.token = ''; store.set('cq_token', ''); } }
    switch (a) {
      case '': await vHome(); break;
      case 'movie': await vMovie(b); break;
      case 'schedule': await vSchedule(); break;
      case 'book': await vBook(b); break;
      case 'pay': await vPay(b); break;
      case 'ticket': await vTicket(b); break;
      case 'orders': await vOrders(); break;
      case 'news': await vNews(); break;
      case 'admin': if (b === 'checkin') await vCheckin(); else if (b === 'reports') await vReports(); else if (b === 'catalog') await vCatalog(); else notFound(); break;
      default: notFound();
    }
  } catch (e) {
    draw('', `<section class="sec wrap"><div class="alert bad"><span>${esc(e.message)}</span><button class="btn" data-act="reload">Thử lại</button></div></section>`);
    A.reload = () => route();
  }
  if (!keep) window.scrollTo(0, 0);
  tick();
}
window.addEventListener('hashchange', () => route());

async function checkGrants() {
  if (!S.user) return;
  try {
    for (const o of await loadOrders()) {
      if (o.fromQueue && o.status === 'HOLD' && !S.grants.has(o.id)) {
        S.grants.add(o.id);
        if (!/^#\/pay\//.test(location.hash)) toast('Có ghế cho bạn từ danh sách chờ. Vào Vé của tôi để thanh toán trước khi hết giờ giữ ghế.');
      }
    }
  } catch { /* bỏ qua */ }
}
setInterval(() => {
  if (document.hidden || $('#modal-root').firstChild) return;
  if (S.refresh) S.refresh().catch(() => {});
  checkGrants();
}, 10000);

document.documentElement.dataset.theme = store.get('cq_theme') || 'dark';
S.token = store.get('cq_token');
route();
