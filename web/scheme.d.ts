interface SchemeOptions {
  print?: (text: string) => void;
  printErr?: (text: string) => void;
}

type RunResult =
  | { kind: "ok" }
  | { kind: "incomplete" }
  | { kind: "error"; message: string };

interface Session {
  run(source: string, emit: (value: string) => void): RunResult;
  delete(): void;
}

interface SchemeModule {
  Session: { new (): Session };
}

declare function createScheme(opts?: SchemeOptions): Promise<SchemeModule>;
