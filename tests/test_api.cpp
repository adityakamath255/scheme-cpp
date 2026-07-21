#include "scheme/session.hpp"

#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

void expect(bool condition) {
  if (!condition) {
    throw std::runtime_error("API test failed");
  }
}

}

int main() {
  scheme::Session session;
  std::vector<scheme::Event> events;
  auto emit = [&](const scheme::Event &event) {
    events.push_back(event);
  };

  std::string source = "(define x 2)\n(display \"value: \")\n(+ x 3)";
  scheme::RunResult result = session.run(source, emit);
  expect(!result.incomplete);
  expect(result.consumed == source.size());
  expect(events.size() == 2);
  expect(std::get<scheme::Output>(events[0]).text == "value: ");
  expect(std::get<scheme::Result>(events[1]).text == "5");

  result = session.run("(+ 1", emit);
  expect(result.incomplete);
  expect(result.consumed == 0);

  try {
    session.execute("(+ 1", emit);
    expect(false);
  } catch (const scheme::EvaluationError &) {
  }

  scheme::Session moved = std::move(session);
  events.clear();
  result = moved.run("x", emit);
  expect(!result.incomplete);
  expect(std::get<scheme::Result>(events.back()).text == "2");

  try {
    moved.execute("(exit 7)", emit);
    expect(false);
  } catch (const scheme::ExitRequest &request) {
    expect(request.status() == 7);
  }
}
