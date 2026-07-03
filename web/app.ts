function byId<T extends HTMLElement>(id: string): T {
  const node = document.getElementById(id);
  if (!node) throw new Error(`missing element #${id}`);
  return node as T;
}

function el(
  tag: string,
  props: Partial<Omit<HTMLElement, "style">>,
  ...children: (Node | string)[]
): HTMLElement {
  const node = Object.assign(document.createElement(tag), props);
  node.append(...children);
  return node;
}

const sample = `(define (square x) (* x x))
(define (sum-of-squares a b)
  (+ (square a) (square b)))

(sum-of-squares 3 4)
(map square '(1 2 3 4 5))
`;

class Console {
  constructor(private readonly root: HTMLElement) {}

  clear() { this.root.replaceChildren(); }
  print(text: string) { this.line(text, "out"); }
  error(text: string) { this.line(text, "err"); }
  result(text: string) { this.line(text, "res"); }
  hint(text: string) { this.line(text, "hint"); }

  private line(text: string, cls: string) {
    this.root.append(el("div", { className: cls, textContent: text }));
    this.root.scrollTop = this.root.scrollHeight;
  }
}

class Editor {
  constructor(
    private readonly node: HTMLTextAreaElement,
    private readonly storageKey: string,
    onRun: () => void,
  ) {
    node.addEventListener("input", () => this.save());
    node.addEventListener("keydown", e => {
      if (e.key === "Enter" && (e.ctrlKey || e.metaKey)) { e.preventDefault(); onRun(); }
      else if (e.key === "Tab") {
        e.preventDefault();
        node.setRangeText("  ", node.selectionStart, node.selectionEnd, "end");
      }
    });
  }

  get value() { return this.node.value; }

  set value(text: string) {
    this.node.value = text;
    this.save();
  }

  get saved(): string | null { return localStorage.getItem(this.storageKey); }
  enable() { this.node.disabled = false; }
  focus() { this.node.focus(); }

  private save() { localStorage.setItem(this.storageKey, this.node.value); }
}

class SplitPane {
  private pct: number;

  constructor(
    private readonly gutter: HTMLElement,
    private readonly storageKey: string,
  ) {
    this.pct = Number(localStorage.getItem(storageKey)) || 50;
    this.apply();
    gutter.addEventListener("pointerdown", e => this.beginDrag(e));
  }

  private apply() {
    document.documentElement.style.setProperty("--split", this.pct + "%");
  }

  private beginDrag(e: PointerEvent) {
    e.preventDefault();
    this.gutter.classList.add("active");
    this.gutter.setPointerCapture(e.pointerId);
    const main = this.gutter.parentElement!;
    const onMove = (ev: PointerEvent) => {
      const r = main.getBoundingClientRect();
      this.pct = Math.min(80, Math.max(20, ((ev.clientX - r.left) / r.width) * 100));
      this.apply();
    };
    const onUp = () => {
      this.gutter.classList.remove("active");
      this.gutter.removeEventListener("pointermove", onMove);
      this.gutter.removeEventListener("pointerup", onUp);
      localStorage.setItem(this.storageKey, this.pct.toFixed(2));
    };
    this.gutter.addEventListener("pointermove", onMove);
    this.gutter.addEventListener("pointerup", onUp);
  }
}

class Permalink {
  static consume(): string | null {
    if (location.hash.length <= 1) return null;
    try {
      const program = this.decode(decodeURIComponent(location.hash.slice(1)));
      history.replaceState(null, "", location.pathname);
      return program;
    } catch { return null; }
  }

  static url(program: string): string {
    return location.origin + location.pathname + "#" + encodeURIComponent(this.encode(program));
  }

  private static encode(s: string): string {
    return btoa(Array.from(new TextEncoder().encode(s), b => String.fromCharCode(b)).join(""));
  }

  private static decode(b: string): string {
    return new TextDecoder().decode(Uint8Array.from(atob(b), c => c.charCodeAt(0)));
  }
}

class Repl {
  private session: Session | undefined;

  constructor(private readonly console: Console) {}

  async boot() {
    const module = await createScheme({
      print: t => this.console.print(t),
      printErr: t => this.console.error(t),
    });
    this.dispose();
    this.session = new module.Session();
  }

  dispose() {
    if (this.session) { try { this.session.delete(); } catch {} }
    this.session = undefined;
  }

  async evaluate(source: string): Promise<number | null> {
    if (!this.session) return null;
    const t0 = performance.now();
    try {
      const r = this.session.run(source, m => {
        if (m.kind === "out") this.console.print(m.text);
        else this.console.result(m.text);
      });
      const ms = performance.now() - t0;
      if (r.kind === "incomplete") this.console.error("unexpected end of input");
      else if (r.kind === "error") this.console.error(r.text);
      return ms;
    } catch (ex) {
      if (ex instanceof RangeError) { this.console.error(ex.message); return performance.now() - t0; }
      this.console.error(ex instanceof Error ? ex.message : String(ex));
      this.console.error("Session crashed and was restarted; defined values are lost.");
      await this.boot();
      return null;
    }
  }
}

class App {
  private readonly console = new Console(byId("output"));
  private readonly repl = new Repl(this.console);
  private readonly editor = new Editor(byId<HTMLTextAreaElement>("editor"), "scheme-program", () => this.run());
  private readonly split = new SplitPane(byId("gutter"), "scheme-split");
  private readonly runBtn = byId<HTMLButtonElement>("run");
  private readonly resetBtn = byId<HTMLButtonElement>("reset");
  private readonly shareBtn = byId<HTMLButtonElement>("share");
  private readonly shareLabel = this.shareBtn.textContent ?? "";
  private shareReset: number | undefined;
  private readonly timing = byId("timing");

  constructor() {
    this.runBtn.addEventListener("click", () => this.run());
    this.resetBtn.addEventListener("click", () => this.reset());
    this.shareBtn.addEventListener("click", () => this.share());
    addEventListener("beforeunload", () => this.repl.dispose());
  }

  async start() {
    await this.reboot();
    this.editor.value = Permalink.consume() ?? this.editor.saved ?? sample;
    this.console.clear();
    this.console.hint("Edit the program, then press Run.");
    this.editor.enable();
    this.shareBtn.disabled = false;
    this.editor.focus();
  }

  private async run() {
    this.console.clear();
    const ms = await this.repl.evaluate(this.editor.value);
    this.timing.textContent = ms === null ? "" : ms < 1 ? "<1 ms" : Math.round(ms) + " ms";
  }

  private async reset() {
    await this.reboot();
    this.console.clear();
    this.console.hint("Fresh session. Press Run to evaluate the program.");
    this.timing.textContent = "";
  }

  private async reboot() {
    this.runBtn.disabled = this.resetBtn.disabled = true;
    await this.repl.boot();
    this.runBtn.disabled = this.resetBtn.disabled = false;
  }

  private async share() {
    const url = Permalink.url(this.editor.value);
    try {
      await navigator.clipboard.writeText(url);
      this.flashShare("Copied");
    } catch {
      history.replaceState(null, "", url);
      this.flashShare("Link in URL");
    }
  }

  private flashShare(label: string) {
    this.shareBtn.textContent = label;
    clearTimeout(this.shareReset);
    this.shareReset = setTimeout(() => { this.shareBtn.textContent = this.shareLabel; }, 1200);
  }
}

new App().start();
