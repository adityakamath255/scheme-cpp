const assert = require("node:assert/strict");
const createScheme = require(process.argv[2]);

createScheme().then(module => {
  const session = new module.Session();
  const messages = [];

  assert.equal(session.run("(define x 40)", () => {}).kind, "ok");
  assert.equal(session.run(
    '(display "web") (+ x 2)',
    message => messages.push([message.kind, message.text]),
  ).kind, "ok");

  assert.deepEqual(messages, [["out", "web"], ["res", "42"]]);
  assert.equal(session.run("(", () => {}).kind, "incomplete");
  assert.equal(session.run("(car 1)", () => {}).kind, "error");

  session.delete();
}).catch(error => {
  console.error(error);
  process.exitCode = 1;
});
