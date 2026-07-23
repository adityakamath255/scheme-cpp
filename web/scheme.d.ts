interface SchemeOptions {
  print?: (text: string) => void;
  printErr?: (text: string) => void;
}

// message kinds are defined in web/main.cpp.
type Message = { kind: "out" | "res"; text: string };

type RunResult =
  | { kind: "ok" }
  | { kind: "incomplete" }
  | { kind: "error"; text: string };

interface Session {
  run(source: string, emit: (m: Message) => void): RunResult;
  delete(): void;
}

interface SchemeModule {
  Session: { new (): Session };
}

declare function createScheme(opts?: SchemeOptions): Promise<SchemeModule>;
