
const pages = window.WIKI_PAGES || [];
const q = document.getElementById('q');
const hits = document.getElementById('hits');
const nav = document.getElementById('nav');

function esc(s) { return s.replace(/[&<>]/g, c => ({'&': '&amp;', '<': '&lt;', '>': '&gt;'}[c])); }

function show(term) {
  if (!term) { hits.innerHTML = ''; nav.style.display = ''; return; }
  const needle = term.toLowerCase();
  const found = [];
  for (const page of pages) {
    const text = page.text;
    let at = text.toLowerCase().indexOf(needle);
    let count = 0;
    while (at !== -1 && count < 3) {
      const from = Math.max(0, at - 50);
      const line = text.slice(from, at + needle.length + 60);
      found.push({ title: page.title, href: page.href,
        snippet: esc(line.slice(0, at - from)) + '<mark>' + esc(line.slice(at - from, at - from + needle.length))
          + '</mark>' + esc(line.slice(at - from + needle.length)) });
      at = text.toLowerCase().indexOf(needle, at + needle.length);
      count += 1;
    }
    if (found.length > 60) { break; }
  }
  nav.style.display = 'none';
  hits.innerHTML = found.length
    ? found.map(f => `<a class="hit" href="${f.href}"><b>${esc(f.title)}</b><span>…${f.snippet}…</span></a>`).join('')
    : '<p>Nothing found.</p>';
}

q.addEventListener('input', () => show(q.value.trim()));
document.addEventListener('keydown', e => {
  if (e.key === '/' && document.activeElement !== q) { e.preventDefault(); q.focus(); }
  if (e.key === 'Escape' && document.activeElement === q) { q.value = ''; show(''); q.blur(); }
});
