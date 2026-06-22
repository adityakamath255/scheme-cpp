// Hand-written types for the Embind boundary exposed by web/main.cpp,
// compiled into scheme.js (global `createScheme`). Kept in sync by hand.

interface SchemeOptions {
  print?: (text: string) => void;
  printErr?: (text: string) => void;
}

// Result of Session.step: one read-and-evaluate of the leading form.
type StepResult =
  | { kind: "value"; rest: string; value?: string }
  | { kind: "incomplete" }
  | { kind: "exhausted" };

interface Session {
  step(source: string): StepResult;
  delete(): void;
}

interface SchemeModule {
  Session: { new (): Session };
}

declare function createScheme(opts?: SchemeOptions): Promise<SchemeModule>;
