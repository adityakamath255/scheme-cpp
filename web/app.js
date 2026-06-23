"use strict";
const el = (id) => document.getElementById(id);
const editor = el("editor");
const output = el("output");
const runBtn = el("run");
const resetBtn = el("reset");
const shareBtn = el("share");
const timing = el("timing");
const gutter = el("gutter");
const sample = `(define (square x) (* x x))
(define (sum-of-squares a b)
  (+ (square a) (square b)))

(sum-of-squares 3 4)
(map square '(1 2 3 4 5))
`;
const b64 = {
    enc: (s) => btoa(String.fromCharCode(...new TextEncoder().encode(s))),
    dec: (b) => new TextDecoder().decode(Uint8Array.from(atob(b), c => c.charCodeAt(0))),
};
const PROGRAM_KEY = "scheme-program";
const SPLIT_KEY = "scheme-split";
function fromHash() {
    if (location.hash.length <= 1)
        return null;
    try {
        return b64.dec(decodeURIComponent(location.hash.slice(1)));
    }
    catch {
        return null;
    }
}
function append(text, cls) {
    const div = document.createElement("div");
    if (cls)
        div.className = cls;
    div.textContent = text;
    output.appendChild(div);
    output.scrollTop = output.scrollHeight;
}
// Most errors are recoverable: interpreter errors throw plain Errors, and a
// too-deep recursion throws a catchable "call stack size exceeded" that leaves
// the session intact. Only genuine memory corruption (out-of-bounds / bad table
// index / unreachable) leaves the module dead, forcing a full reboot.
function isFatal(message) {
    return /out of bounds|memory access|table index|unreachable/i.test(message);
}
function run() {
    if (!session)
        return;
    output.replaceChildren();
    let buffer = editor.value;
    const t0 = performance.now();
    try {
        while (buffer.trim()) {
            const r = session.step(buffer);
            if (r.kind === "incomplete") {
                append("unexpected end of input", "err");
                return;
            }
            if (r.kind === "exhausted")
                return;
            if (r.value !== undefined)
                append(r.value, "res");
            buffer = r.rest;
        }
    }
    catch (ex) {
        const message = ex instanceof Error ? ex.message : String(ex);
        append(message, "err");
        if (isFatal(message)) {
            append("Session crashed and was restarted; defined values are lost.", "err");
            boot();
        }
    }
    finally {
        const dt = performance.now() - t0;
        timing.textContent = dt < 1 ? "<1 ms" : Math.round(dt) + " ms";
    }
}
let session;
function disposeSession() {
    if (session) {
        try {
            session.delete();
        }
        catch { }
    }
}
async function boot() {
    runBtn.disabled = resetBtn.disabled = true;
    const fresh = await createScheme({ print: t => append(t, "out"), printErr: t => append(t, "err") });
    disposeSession();
    session = new fresh.Session();
    runBtn.disabled = resetBtn.disabled = false;
}
async function reset() {
    await boot();
    output.replaceChildren();
    timing.textContent = "";
    append("Fresh session. Press Run to evaluate the program.", "hint");
}
async function share() {
    const url = location.origin + location.pathname + "#" + encodeURIComponent(b64.enc(editor.value));
    history.replaceState(null, "", url);
    try {
        await navigator.clipboard.writeText(url);
        shareBtn.textContent = "Copied";
    }
    catch {
        shareBtn.textContent = "Link in URL";
    }
    setTimeout(() => { shareBtn.textContent = "Copy link"; }, 1200);
}
// The hash is a one-shot import channel: read it, fold it into the persistent
// store, then strip it so a later reload doesn't shadow newer local edits.
boot().then(() => {
    const shared = fromHash();
    editor.value = shared ?? localStorage.getItem(PROGRAM_KEY) ?? sample;
    if (shared !== null) {
        localStorage.setItem(PROGRAM_KEY, shared);
        history.replaceState(null, "", location.pathname);
    }
    output.replaceChildren();
    append("Edit the program, then press Run.", "hint");
    editor.disabled = shareBtn.disabled = false;
    editor.focus();
});
addEventListener("beforeunload", disposeSession);
runBtn.addEventListener("click", run);
resetBtn.addEventListener("click", reset);
shareBtn.addEventListener("click", share);
editor.addEventListener("input", () => localStorage.setItem(PROGRAM_KEY, editor.value));
editor.addEventListener("keydown", e => {
    if (e.key === "Enter" && (e.ctrlKey || e.metaKey)) {
        e.preventDefault();
        run();
    }
    else if (e.key === "Tab") {
        e.preventDefault();
        const { selectionStart: s, selectionEnd: t } = editor;
        editor.setRangeText("  ", s, t, "end");
    }
});
function setSplit(value) {
    document.documentElement.style.setProperty("--split", value);
}
function getSplit() {
    return document.documentElement.style.getPropertyValue("--split");
}
const savedSplit = localStorage.getItem(SPLIT_KEY);
if (savedSplit)
    setSplit(savedSplit);
gutter.addEventListener("pointerdown", e => {
    e.preventDefault();
    gutter.classList.add("active");
    gutter.setPointerCapture(e.pointerId);
    const main = gutter.parentElement;
    if (!main)
        return;
    const onMove = (ev) => {
        const r = main.getBoundingClientRect();
        const pct = Math.min(80, Math.max(20, ((ev.clientX - r.left) / r.width) * 100));
        setSplit(pct.toFixed(2) + "%");
    };
    const onUp = () => {
        gutter.classList.remove("active");
        gutter.removeEventListener("pointermove", onMove);
        gutter.removeEventListener("pointerup", onUp);
        localStorage.setItem(SPLIT_KEY, getSplit());
    };
    gutter.addEventListener("pointermove", onMove);
    gutter.addEventListener("pointerup", onUp);
});
