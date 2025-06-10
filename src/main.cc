#include <dtl/dtl.hpp>
#include <filesystem>
#include <nix/args.hh>
#include <nix/attr-path.hh>
#include <nix/attr-set.hh>
#include <nix/common-eval-args.hh>
#include <nix/eval-gc.hh>
#include <nix/eval.hh>
#include <nix/eval-inline.hh>
#include <nix/eval-settings.hh>
#include <nix/flake/flake.hh>
#include <nix/globals.hh>
#include <nix/nixexpr.hh>
#include <nix/shared.hh>
#include <nix/signals.hh>
#include <nix/store-api.hh>
#include <nix/symbol-table.hh>
#include <nix/terminal.hh>
#include <nix/types.hh>
#include <nix/util.hh>
#include <nix/value.hh>
#include <string>
#include <utility>
#include <variant>

using nix::Attr;
using nix::Bindings;
using nix::EvalState;
using nix::PrintOptions;
using nix::Strings;
using nix::Symbol;
using nix::SymbolStr;
using nix::Value;
using nix::ValuePrinter;

typedef std::set<const void *> ValuesSeen;

class Tagged {
  public:
  ValuesSeen * seen;
  EvalState * state;
  Value * value;
  Tagged(const Tagged & tagged, Value * value) : value(value) {
    seen = tagged.seen;
    state = tagged.state;
  }
  Tagged(ValuesSeen * seen, EvalState * state, Value * value) : seen(seen), state(state), value(value) { }
};

bool isTTY = isatty(fileno(stdout));

class Change {
  friend std::ostream & operator << (std::ostream & output, const Change & change);
  private:
    std::string deletion;
    std::string addition;
  public:
    Change(std::string deletion, std::string addition) : deletion(deletion), addition(addition) {}
};

std::ostream & operator << (std::ostream & output, const Change & change) {
  if (!change.deletion.empty()) {
    if (isTTY) {
      std::cout << "\x1b[31m-" << change.deletion << "\x1b[0m\n";
    } else {
      std::cout << "-" << change.deletion << "\n";
    }
  }
  if (!change.addition.empty()) {
    if (isTTY) {
      std::cout << "\x1b[32m+" << change.addition << "\x1b[0m\n";
    } else {
      std::cout << "+" << change.addition << "\n";
    }
  }
  return output;
}

void printChange(std::string deletion, std::string addition) {
  std::cout.flush();
  nix::checkInterrupt();
  std::cout << Change(deletion, addition);
}

void printUniDiff(dtl::Diff<std::string, std::vector<std::string>> diff) {
  std::cout.flush();
  nix::checkInterrupt();
  auto hunks = diff.getUniHunks();
  for (auto hunk = hunks.begin(); hunk != hunks.end(); ++hunk) {
    if (isTTY) {
      std::cout << "\x1b[36m   @@" << " -"  << hunk->a << "," << hunk->b << " +"  << hunk->c << "," << hunk->d << " @@" << "\x1b[0m\n";
    } else {
      std::cout << "  @@" << " -"  << hunk->a << "," << hunk->b << " +"  << hunk->c << "," << hunk->d << " @@" << "\n";
    }
    for (auto common = hunk->common[0].begin(); common != hunk->common[0].end(); ++common) {
      std::cout << "    " << common->first << "\n";
    }
    for (auto change = hunk->change.begin(); change != hunk->change.end(); ++change) {
      switch (change->second.type) {
        case dtl::SES_ADD:
          if (isTTY) {
            std::cout << "\x1b[32m   +" << change->first << "\x1b[0m\n";
          } else {
            std::cout << "   +" << change->first << "\n";
          }
          break;
        case dtl::SES_DELETE:
          if (isTTY) {
            std::cout << "\x1b[31m   -" << change->first << "\x1b[0m\n";
          } else {
            std::cout << "   -" << change->first << "\n";
          }
          break;
        case dtl::SES_COMMON:
          std::cout << "    " << change->first << "\n";
          break;
      }
    }
    for (auto common = hunk->common[1].begin(); common != hunk->common[1].end(); ++common) {
      std::cout << "   " << common->first << "\n";
    }
  }
}

std::ostream &
printString(bool printDeletion, std::ostream & str, Tagged & v) {
  const std::string_view string = v.value->string_view();
  if (string.find('\n') == string.npos) {
    str << ValuePrinter(*v.state, *v.value, PrintOptions {});
    return str;
  };
  if (printDeletion) {
    if (isTTY) {
      str << "''\x1b[0m\n\x1b[31m  ";
    } else {
      str << "''\n  ";
    }
  } else {
    if (isTTY) {
      str << "''\x1b[0m\n\x1b[32m  ";
    } else {
      str << "''\n  ";
    }
  }
  for (auto i = string.begin(); i != string.end(); ++i) {
    if (*i == '\'' && *(i+1) == '\'') {
      str << "'''";
      ++i;
    } else if (*i == '\n') {
      if (printDeletion)
        if (isTTY) {
          str << "\x1b[0m\n\x1b[31m  ";
        } else {
          str << "\n  ";
        }
      else
        if (isTTY) {
          str << "\x1b[0m\n\x1b[32m  ";
        } else {
          str << "\n  ";
        }
    } else
      str << *i;
  }
  if (printDeletion) {
    if (isTTY) {
      str << "\x1b[31m''";
    } else {
      str << "''";
    }
  } else {
    if (isTTY) {
      str << "\x1b[32m''";
    } else {
      str << "''";
    }
  }
  return str;
}

void printAttrs(bool printDeletion, const std::string & path, Tagged & v);

std::string serializeScalar(bool printDeletion, Tagged & v, PrintOptions options) {
  std::stringstream ss;
  if (v.value->type() == nix::nString) {
    printString(printDeletion, ss, v);
  } else {
    ss << ValuePrinter(*v.state, *v.value, options);
  }
  std::string s = ss.str();
  return s;
}

Tagged parseAndEval(ValuesSeen & seen, EvalState & state, Value & value, const std::string & expression, const std::string & path) {
  state.eval(state.parseExprFromString(expression, state.rootPath(path)), value);
  return Tagged(&seen, &state, &value);
}

void diffAttrs(const std::string & path, Tagged & v, Tagged & w);

bool equals(Tagged & v, Tagged & w);

void diffLists(const std::string & path, Tagged & v, Tagged & w);

void diffStrings(const std::string & path, Tagged & v, Tagged & w);

PrintOptions printDrv = PrintOptions { .force = true, .derivationPaths = true, };

void diffValues(const std::string & path, Tagged & v, Tagged & w) {
  auto vSeen = !v.seen->insert(v.value).second;
  auto wSeen = !w.seen->insert(w.value).second;

  if (path.ends_with(".type")) {
    v.state->forceValue(*v.value, v.value->determinePos(nix::noPos));
    w.state->forceValue(*w.value, w.value->determinePos(nix::noPos));
  }

  if (v.value->type() == nix::nThunk && w.value->type() == nix::nThunk) {
  } else if (v.value->type() == nix::nThunk) {
    if (w.value->type() == nix::nAttrs && !(w.state->isDerivation(*w.value))) {
      printAttrs(false, path, w);
    } else {
      printChange(
        "",
        path + " = " + serializeScalar(false, w, PrintOptions {}) + ";"
      );
    }
  } else if (w.value->type() == nix::nThunk) {
    if (v.value->type() == nix::nAttrs && !(v.state->isDerivation(*v.value))) {
      printAttrs(true, path, v);
    } else {
      printChange(
        path + " = " + serializeScalar(true, v, PrintOptions {}) + ";",
        ""
      );
    }
  } else if (v.value->type() == nix::nAttrs && w.value->type() == nix::nAttrs && v.state->isDerivation(*v.value) && w.state->isDerivation(*w.value)) {
    if (!equals(v, w)) {
      printChange(
        path + " = " + serializeScalar(true, v, printDrv) + ";",
        path + " = " + serializeScalar(false, w, printDrv) + ";"
      );
    }
  } else if (vSeen && wSeen) { // TODO seen
  } else if (vSeen) { // TODO seen
    /*
    printChange(
      path + " = " + serializeScalar(true, v, PrintOptions {}) + ";",
      path + " = " + serializeScalar(false, w, PrintOptions {}) + ";"
    );*/
  } else if (wSeen) { // TODO seen
    /*printChange(
      path + " = " + serializeScalar(true, v, PrintOptions {}) + ";",
      path + " = " + serializeScalar(false, w, PrintOptions {}) + ";"
    );*/
  } else if (v.value->type() == w.value->type()) {
    switch (v.value->type()) {
      case nix::nAttrs:
        diffAttrs(path, v, w);
        break;
      case nix::nList:
        diffLists(path, v, w);
        break;
      case nix::nString:
        if (!equals(v, w)) {
          diffStrings(path, v, w);
        }
        break;
      default:
        if (!equals(v, w)) {
          printChange(
            path + " = " + serializeScalar(true, v, PrintOptions {}) + ";",
            path + " = " + serializeScalar(false, w, PrintOptions {}) + ";"
          );
        }
        break;
    }
  } else {
    printChange(
      path + " = " + serializeScalar(true, v, PrintOptions {}) + ";",
      path + " = " + serializeScalar(false, w, PrintOptions {}) + ";"
    );
  }
}

// From nix/src/nix/repl.cc
bool isVarName(const std::string_view & s) {
    if (s.size() == 0) return false;
    if (nix::isReservedKeyword(s)) return false;
    char c = s[0];
    if ((c >= '0' && c <= '9') || c == '-' || c == '\'') return false;
    for (auto & i : s)
        if (!((i >= 'a' && i <= 'z') ||
              (i >= 'A' && i <= 'Z') ||
              (i >= '0' && i <= '9') ||
              i == '_' || i == '-' || i == '\''))
            return false;
    return true;
}

std::string quoteAttribute(const std::string_view & attribute) {
    if (isVarName(attribute)) {
        return std::string(attribute);
    }
    std::ostringstream buf;
    nix::printLiteralString(buf, attribute);
    return buf.str();
}

const std::string appendPath(const std::string & prefix, const std::string_view & suffix) {
    if (prefix.empty()) {
        return quoteAttribute(suffix);
    }
    return prefix + "." + quoteAttribute(suffix);
}

void printValue(bool printDeletion, const std::string & path, Tagged & v);

void printAttrs(bool printDeletion, const std::string & path, Tagged & v) {
  std::vector<Symbol> ks;
  for (auto & i : *v.value->attrs()) { // TODO attrs -> names
    ks.emplace_back(i.name);
  }
  std::sort(ks.begin(), ks.end());
  auto last = std::unique(ks.begin(), ks.end());
  ks.erase(last, ks.end());

  for (auto & i : ks) {
    SymbolStr name = v.state->symbols[i];
    const Attr * x = v.value->attrs()->get(i);
    Tagged t = Tagged(v, x->value);
    printValue(printDeletion, appendPath(path, name), t);
  }
}

void printList(bool printDeletion, const std::string & path, Tagged & v) {
  auto xs = v.value->listItems();
  long unsigned int n = xs.size();
  for (long unsigned int i = 0; i < n; i++ ) {
    Tagged t = Tagged(v, xs[i]);
    printValue(printDeletion, appendPath(path, std::format(".{}", i)), t);
  }
}

void printValue(bool printDeletion, const std::string & path, Tagged & v) {
  if (v.value->type() == nix::nThunk) {
  } else if (v.value->type() == nix::nAttrs && v.state->isDerivation(*v.value)) {
    printChange(
      path + " = " + serializeScalar(true, v, printDrv) + ";",
      ""
    );
  } else if (v.value->type()) {
    switch (v.value->type()) {
      case nix::nAttrs:
        printAttrs(printDeletion, path, v);
        break;
      case nix::nList:
        printList(printDeletion, path, v);
        break;
      default:
        if (printDeletion) {
          printChange(path + " = " + serializeScalar(true, v, PrintOptions {}) + ";", "");
        } else {
          printChange("", path + " = " + serializeScalar(false, v, PrintOptions {}) + ";");
        }
        break;
    }
  }
}

bool equals(Tagged & v, Tagged & w) {
  return serializeScalar(true, v, printDrv) == serializeScalar(true, w, printDrv);
}

void diffAttrs(const std::string & path, Tagged & v, Tagged & w) {
  std::map<std::string, Symbol> ks, ls;
  std::vector<std::string> names;

  for (auto & i : *v.value->attrs()) { // TODO attrs -> names
    auto name = std::string(v.state->symbols[i.name]);
    ks[name] = i.name;
    names.emplace_back(name);
  }
  for (auto & i : *w.value->attrs()) { // TODO attrs -> names
    auto name = std::string(w.state->symbols[i.name]);
    ls[name] = i.name;
    names.emplace_back(name);
  }
  std::sort(names.begin(), names.end());
  auto last = std::unique(names.begin(), names.end());
  names.erase(last, names.end());

  for (auto & name : names) {
    auto i = ks[name];
    auto j = ls[name];
    const Attr * x = v.value->attrs()->get(i);
    const Attr * y = w.value->attrs()->get(j);
    if (x && y) {
      Tagged t = Tagged(v, x->value);
      Tagged s = Tagged(w, y->value);
      diffValues(appendPath(path, name), t, s);
    } else if (x) {
      Tagged t = Tagged(v, x->value);
      printValue(true, appendPath(path, name), t);
    } else if (y) {
      Tagged s = Tagged(w, y->value);
      printValue(false, appendPath(path, name), s);
    }
  }
}

void diffLists(const std::string & path, Tagged & v, Tagged & w) {
  auto xs = v.value->listItems();
  auto ys = v.value->listItems();
  long unsigned int n = std::min(xs.size(), ys.size());
  for (long unsigned int i = 0; i < n; i++ ) {
    Tagged t = Tagged(v, xs[i]);
    Tagged s = Tagged(w, ys[i]);
    diffValues(appendPath(path, std::format(".{}", i)), t, s);
  }
  for (long unsigned int i = n; i < xs.size(); i++) {
    Tagged t = Tagged(v, xs[i]);
    printChange(path + "." + std::to_string(i) + " = " + serializeScalar(true, t, PrintOptions {}) + ";", "");
  }
  for (long unsigned int i = n; i < xs.size(); i++) {
    Tagged s = Tagged(w, ys[i]);
    printChange("", path + "." + std::to_string(i) + " = " + serializeScalar(false, s, PrintOptions {}) + ";");
  }
}

std::vector<std::string> splitLines(const std::string & string) {
  auto result = std::vector<std::string>{};
  auto ss = std::stringstream{string};
  for (std::string line; std::getline(ss, line, '\n');) result.push_back(line);
  return result;
}

void diffStrings(const std::string & path, Tagged & v, Tagged & w) {
  if (v.value->string_view().find("\n") == std::string::npos && v.value->string_view().find("\n") == std::string::npos) {
    printChange(
      path + " = " + serializeScalar(true, v, PrintOptions {}) + ";",
      path + " = " + serializeScalar(false, w, PrintOptions {}) + ";"
    );
    return;
  }
  dtl::Diff<std::string, std::vector<std::string>> diff(
    splitLines(std::string(v.value->string_view())),
    splitLines(std::string(w.value->string_view()))
  );
  diff.compose();
  diff.composeUnifiedHunks();
  std::cout << " " << path << " =\n";
  printUniDiff(diff);
}

class BaseExpr {
  std::string string;
  public:
    std::string to_string() {
      return string;
    };
    BaseExpr(std::string aString) {
      string = aString;
    };
};

class FinalExpr {
  std::string string;
  public:
    std::string to_string() {
      return string;
    };
    FinalExpr(BaseExpr e, std::optional<std::string> rootPath) {
      if (rootPath.has_value()) {
        string = std::format("let inherit ({}) config system; in builtins.seq system config.{}", e.to_string(), rootPath.value());
      } else {
        string = std::format("let inherit ({}) config system; in builtins.seq system config", e.to_string());
      }
    }
};

class FlakeURL {
  std::string path;
  std::string attr;
  public:
  FlakeURL(std::string path, std::string attr) : path{path}, attr{attr} {}
  std::string to_string() { return std::format("{}#{}", path, attr); }
  BaseExpr toBaseExpr() {
    return BaseExpr(std::format("let inherit ((builtins.getFlake (toString {})).{}) config; in {{ inherit config; system = config.system.build.toplevel; }}", path, attr));
  }
};

std::optional<FlakeURL> maybeParseFlakeURL(std::string flakeURL) {
  auto attrStart = flakeURL.find("#");
  if (attrStart == std::string::npos) {
    return std::nullopt;
  }
  auto path = flakeURL.substr(0, attrStart);
  auto attr = flakeURL.substr(attrStart + 1);
  return FlakeURL(path, attr);
}

class Path {
  std::string path;
  public:
  std::string to_string() {
    return path;
  };
  Path(std::string path) : path{path} { }
  BaseExpr toBaseExpr() {
    return BaseExpr("import <nixpkgs/nixos> { configuration = (" + path + "); }");
  }
};

class Expr {
  std::string expr;
  public:
  std::string to_string() {
    return expr;
  };
  Expr(std::string expr) : expr{expr} {}
  BaseExpr toBaseExpr() { return BaseExpr(expr); }
};

template<class... Ts>
struct overloads : Ts... { using Ts::operator()...; };

class ConfigExpr {
  public:
  std::variant<Path, Expr, FlakeURL> variant;
  ConfigExpr(auto variant) : variant{variant} {}

  ConfigExpr inferredConfigExpr(std::filesystem::path workTree) {
    auto path = std::get<Path>(variant);
    if (std::holds_alternative<Path>(variant))
      return ConfigExpr(Path(std::format("./{}/{}", workTree.string(), std::get<Path>(variant).to_string())));
    return variant;
  }

  std::string to_string() {
    return std::visit(overloads {
        [](Path path) { return path.to_string(); },
        [](Expr expr) { return expr.to_string(); },
        [](FlakeURL flakeURL) { return flakeURL.to_string(); }
    }, variant);
  }

  BaseExpr toBaseExpr() {
    return std::visit(overloads {
        [](Path path) { return (Path(path)).toBaseExpr(); },
        [](Expr expr) { return expr.toBaseExpr(); },
        [](FlakeURL flakeURL) { return flakeURL.toBaseExpr(); }
    }, variant);
  }
};

std::filesystem::path workTree;

int main(int argc, char ** argv) {
  bool expr = false;
  std::string rev = "HEAD";
  std::optional<std::string> rootPath;
  std::optional<ConfigExpr> maybeConfig1Expr, maybeConfig2Expr;

  struct MyArgs : nix::LegacyArgs, nix::MixEvalArgs
  {
      using nix::LegacyArgs::LegacyArgs;
  };

  MyArgs myArgs(std::string(nix::baseNameOf(argv[0])), [&](Strings::iterator & arg, const Strings::iterator & end) {
    if (*arg == "--help") {
        nix::showManPage("nixos-diff");
    } else if (*arg == "--version") {
        nix::printVersion("nixos-diff");
    } else if (*arg == "--color=always") {
        isTTY = true;
    } else if (*arg == "--expr") {
        expr = true;
    } else if (*arg == "--rev") {
        rev = nix::getArg(*arg, arg, end);
    } else if (*arg == "-p" || *arg == "--path") {
        rootPath = nix::getArg(*arg, arg, end);
    } else {
      if (!maybeConfig1Expr.has_value()) {
        maybeConfig1Expr = Path(*arg);
      } else if (!maybeConfig2Expr.has_value()) {
        maybeConfig2Expr = Path(*arg);
      } else {
        std::cerr << "error: " << " " << maybeConfig1Expr.value().to_string() << " " << maybeConfig2Expr.value().to_string() << *arg << "\n";
        return false;
      }
    }
    return true;
  });

  nix::initNix();
  nix::initGC();
  nix::flake::initLib(nix::flakeSettings);

  myArgs.parseCmdline(nix::argvToStrings(argc, argv));

  auto store = nix::openStore();
  auto evalStore = myArgs.evalStoreUrl
    ? nix::openStore(*myArgs.evalStoreUrl)
    : nix::openStore();
  auto state1 = nix::make_ref<nix::EvalState>(
    myArgs.lookupPath,
    evalStore,
    nix::fetchSettings,
    nix::evalSettings
  );
  auto state2 = nix::make_ref<nix::EvalState>(
    myArgs.lookupPath,
    evalStore,
    nix::fetchSettings,
    nix::evalSettings
  );

  ConfigExpr config1Expr = maybeConfig1Expr.value();
  auto maybeFlakeURL1 = maybeParseFlakeURL(config1Expr.to_string());
  auto flake = maybeFlakeURL1.has_value();

  if (flake) {
    config1Expr = maybeFlakeURL1.value();
    if (maybeConfig2Expr.has_value()) {
      maybeConfig2Expr = maybeParseFlakeURL(maybeConfig2Expr.value().to_string()).value();
    }
  } else if (expr) {
    config1Expr = ConfigExpr(Expr(config1Expr.to_string()));
    if (maybeConfig2Expr.has_value()) {
      maybeConfig2Expr = ConfigExpr(Expr(maybeConfig2Expr.value().to_string()));
    }
  }

  if (!maybeConfig2Expr.has_value()) {
    std::atexit([] () { (void)!system(std::format("git worktree remove -f {}", workTree.string()).c_str()); });
    std::filesystem::path tmpDir = "tmp";
    workTree = tmpDir;
    auto exitCode = system(std::format("git worktree add --quiet {}", workTree.string()).c_str());
    assert(exitCode == 0);
  }

  if (!maybeConfig2Expr.has_value()) {
    maybeConfig2Expr = config1Expr.inferredConfigExpr(workTree);
  }
  ConfigExpr config2Expr = maybeConfig2Expr.value();

  FinalExpr finalExpr1 = FinalExpr(config1Expr.toBaseExpr(), rootPath);
  FinalExpr finalExpr2 = FinalExpr(config2Expr.toBaseExpr(), rootPath);

  ValuesSeen seen1, seen2;
  Value value1, value2;
  Tagged config1 = parseAndEval(seen1, *state1, value1, finalExpr1.to_string(), ".");
  Tagged config2 = parseAndEval(seen2, *state2, value2, finalExpr2.to_string(), workTree.empty() ? "." : workTree);

  if (workTree.empty()) {
    diffValues(rootPath.value_or(""), config1, config2);
  } else {
    diffValues(rootPath.value_or(""), config2, config1);
  }

  return 0;
}
