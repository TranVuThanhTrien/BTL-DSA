// Giao diện web. Người phụ trách: E
// Chỉ nói chuyện với server qua API (hợp đồng ghi ở đầu src/server/main_server.cpp).
// Không viết nghiệp vụ ở đây: chọn ghế, hàng chờ, hoàn tiền đều do C++ quyết định.
'use strict';

const $ = (sel) => document.querySelector(sel);
const esc = (s) => String(s).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
const money = (n) => Number(n).toLocaleString('vi-VN') + ' ₫';

async function api(method, url, body) {
  const res = await fetch(url, {
    method,
    headers: body ? { 'Content-Type': 'application/json' } : undefined,
    body: body ? JSON.stringify(body) : undefined,
  });
  const data = await res.json().catch(() => ({}));
  if (!res.ok) throw new Error(data.error || 'Lỗi ' + res.status);
  return data;
}

let toastTimer = null;
function toast(msg) {
  const el = $('#toast');
  el.textContent = msg;
  el.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { el.hidden = true; }, 4500);
}

const state = { selected: null }; // { row, col } ghế đang chọn

async function loadSeats() {
  const data = await api('GET', '/api/seats');
  const grid = $('#seat-grid');
  grid.style.setProperty('--cols', data.cols);
  grid.innerHTML = '';
  data.seats.forEach((row, r) => {
    row.forEach((seat, c) => {
      const b = document.createElement('button');
      b.className = 'seat' + (seat.type === 'VIP' ? ' vip' : '') + (seat.taken ? ' taken' : '');
      b.textContent = c + 1;
      b.setAttribute('aria-label', 'Ghế ' + String.fromCharCode(65 + r) + (c + 1) + (seat.taken ? ', đã đặt' : ', trống'));
      if (!seat.taken) b.addEventListener('click', () => selectSeat(r, c));
      grid.appendChild(b);
    });
  });
  // TODO: tô màu ghế đang chọn (class "selected"), hiện số ghế còn trống từ data.free
}

function selectSeat(r, c) {
  state.selected = { row: r, col: c };
  // TODO: vẽ lại để ghế (r, c) có class "selected"; đổi ô "Loại vé" cho khớp loại ghế
}

async function loadQueue() {
  const data = await api('GET', '/api/queue');
  const box = $('#queue');
  if (!data.items.length) { box.innerHTML = '<p class="text-slate-500">Không có ai đang chờ.</p>'; return; }
  // TODO: vẽ mỗi khách thành một nút, nối bằng ký hiệu ⇄, gắn nhãn HEAD ở đầu và TAIL ở cuối
  box.innerHTML = data.items.map((it) => '<span class="card">' + esc(it.name) + ' · ' + esc(it.type) + '</span>').join('');
}

async function book() {
  const name = $('#name').value.trim();
  if (!name) { toast('Nhập họ tên trước khi đặt vé.'); $('#name').focus(); return; }
  try {
    const r = await api('POST', '/api/book', { name, type: $('#type').value });
    if (r.seated) toast('Đã giữ ghế ' + r.ticket.seat + '. Giá ' + money(r.ticket.price) + '.');
    else toast('Hết ghế. Bạn đang ở vị trí ' + r.queuePosition + ' trong hàng chờ.');
    // TODO: nếu có vé thì mở khung thanh toán (POST /api/pay)
    await refresh();
  } catch (e) { toast(e.message); }
}

async function refresh() { await Promise.all([loadSeats(), loadQueue()]); }

$('#btn-book').addEventListener('click', book);
refresh().catch((e) => toast(e.message));

// TODO (E, tùy chọn): xem trước giao diện khi chưa đăng nhập, chỉ hỏi đăng nhập khi bấm "Đặt vé";
//                     trang Tin tức. Cần thêm API tương ứng, bàn với A trước.

// TODO (E, đóng gói): khi fetch thất bại (server đã tắt), hiện thông báo "Mất kết nối, hãy mở lại ứng dụng" thay vì để trống.
