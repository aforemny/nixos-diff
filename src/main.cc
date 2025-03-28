#include <filesystem>
#include <nix/args.hh>
#include <nix/attr-path.hh>
#include <nix/attr-set.hh>
#include <nix/common-eval-args.hh>
#include <nix/eval-gc.hh>
#include <nix/eval.hh>
#include <nix/eval-inline.hh>
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

static ValuesSeen seen1;
static ValuesSeen seen2;

struct Context {
    Context(EvalState & state, Bindings & autoArgs, Value config1, Value config2)
        : state(state), autoArgs(autoArgs), config1(config1), config2(config2),
          underscoreType(state.symbols.create("_type"))
    {}
    EvalState & state;
    Bindings & autoArgs;
    Value config1;
    Value config2;
    Symbol underscoreType;
};

//bool isTTY = nix::isTTY();
bool isTTY = true;

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

std::string serializeValue(Context & ctx, Value & v, PrintOptions options) {
  std::stringstream ss;
  ss << ValuePrinter(ctx.state, v, options);
  std::string s = ss.str();
  return s;
}

Value parseAndEval(EvalState & state, const std::string & expression, const std::string & path) {
  Value v {};
  state.eval(state.parseExprFromString(expression, state.rootPath(path)), v);
  return v;
}

void diffAttrs(Context & ctx, const std::string & path, Value & v, Value & w);

bool equals(Context & state, Value & v, Value & w);

void diffLists(Context & ctx, const std::string & path, Value & v, Value & w);

PrintOptions printDrv = PrintOptions { .force = true, .derivationPaths = true, };

void diffValues(Context & ctx, const std::string & path, Value & v, Value & w) {
  auto vSeen = !seen1.insert(&v).second;
  auto wSeen = !seen2.insert(&w).second;
  if (v.type() == nix::nThunk && w.type() == nix::nThunk) {
  } else if (v.type() == nix::nThunk) {
    printChange(
      "",
      path + " = " + serializeValue(ctx, w, PrintOptions {}) + ";"
    );
  } else if (w.type() == nix::nThunk) {
    printChange(
      path + " = " + serializeValue(ctx, v, PrintOptions {}) + ";",
      ""
    );
  } else if (v.type() == nix::nAttrs && w.type() == nix::nAttrs && ctx.state.isDerivation(v) && ctx.state.isDerivation(w)) {
    if (!equals(ctx, v, w)) {
      printChange(
        path + " = " + serializeValue(ctx, v, printDrv) + ";",
        path + " = " + serializeValue(ctx, w, printDrv) + ";"
      );
    }
  } else if (vSeen && wSeen) { // TODO
  } else if (vSeen) { // TODO
    /*
    printChange(
      path + " = " + serializeValue(ctx, v, PrintOptions {}) + ";",
      path + " = " + serializeValue(ctx, w, PrintOptions {}) + ";"
    );*/
  } else if (wSeen) { // TODO
    /*printChange(
      path + " = " + serializeValue(ctx, v, PrintOptions {}) + ";",
      path + " = " + serializeValue(ctx, w, PrintOptions {}) + ";"
    );*/
  } else if (v.type() == w.type()) {
    switch (v.type()) {
      case nix::nAttrs:
        diffAttrs(ctx, path, v, w);
        break;
      case nix::nList:
        diffLists(ctx, path, v, w);
        break;
      case nix::nThunk: // equals forces
        break;
      default:
        if (!equals(ctx, v, w)) {
          printChange(
            path + " = " + serializeValue(ctx, v, PrintOptions {}) + ";",
            path + " = " + serializeValue(ctx, w, PrintOptions {}) + ";"
          );
        }
        break;
    }
  } else {
    printChange(
      path + " = " + serializeValue(ctx, v, PrintOptions {}) + ";",
      path + " = " + serializeValue(ctx, w, PrintOptions {}) + ";"
    );
  }
}

bool equals(Context & ctx, Value & v, Value & w) {
  return serializeValue(ctx, v, printDrv) == serializeValue(ctx, w, printDrv);
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

void diffAttrs(Context & ctx, const std::string & path, Value & v, Value & w) {
  std::vector<Symbol> ks;
  for (auto & i : *v.attrs()) { // TODO attrs -> names
    ks.emplace_back(i.name);
  }
  for (auto & i : *w.attrs()) { // TODO attrs -> names
    ks.emplace_back(i.name);
  }
  std::sort(ks.begin(), ks.end());
  auto last = std::unique(ks.begin(), ks.end());
  ks.erase(last, ks.end());

  for (auto & i : ks) {
    SymbolStr name = ctx.state.symbols[i];
    const Attr * x = v.attrs()->get(i);
    const Attr * y = w.attrs()->get(i);
    if (x && y) {
      diffValues(ctx, appendPath(path, name), *x->value, *y->value);
    } else if (x) {
      printChange(appendPath(path, name) + " = " + serializeValue(ctx, *x->value, PrintOptions {}) + ";", "");
    } else if (y) {
      printChange("", appendPath(path, name) + " = " + serializeValue(ctx, *y->value, PrintOptions {}) + ";");
    }
  }
}

void diffLists(Context & ctx, const std::string & path, Value & v, Value & w) {
  auto xs = v.listItems();
  auto ys = v.listItems();
  long unsigned int n = std::min(xs.size(), ys.size());
  for (long unsigned int i = 0; i < n; i++ ) {
    diffValues(ctx, appendPath(path, std::format(".{}", i)), *xs[i], *ys[i]);
  }
  for (long unsigned int i = n; i < xs.size(); i++) {
    printChange(path + "." + std::to_string(i) + " = " + serializeValue(ctx, *xs[i], PrintOptions {}) + ";", "");
  }
  for (long unsigned int i = n; i < xs.size(); i++) {
    printChange("", path + "." + std::to_string(i) + " = " + serializeValue(ctx, *ys[i], PrintOptions {}) + ";");
  }
}

void printDiff(Context & ctx) {
  Value & v = ctx.config1;
  Value & w = ctx.config2;

  diffValues(ctx, "", v, w);
}

std::filesystem::path workTree;

int main(int argc, char ** argv) {
  bool expr = false;
  std::string rev = "HEAD";
  std::string config1Expr, config2Expr;

  struct MyArgs : nix::LegacyArgs, nix::MixEvalArgs
  {
      using nix::LegacyArgs::LegacyArgs;
  };

  MyArgs myArgs(std::string(nix::baseNameOf(argv[0])), [&](Strings::iterator & arg, const Strings::iterator & end) {
    if (*arg == "--help") {
        nix::showManPage("nixos-diff");
    } else if (*arg == "--version") {
        nix::printVersion("nixos-diff");
    } else if (*arg == "--expr") {
        expr = true;
    } else if (*arg == "--rev") {
        rev = nix::getArg(*arg, arg, end);
        std::cout << "rev: " << rev << "\n";
    } else {
      if (config1Expr.empty()) {
        config1Expr = *arg;
      } else if (config2Expr.empty()) {
        config2Expr = *arg;
      } else {
        std::cerr << "error: " << " " << config1Expr << " " << config2Expr << *arg << "\n";
        return false;
      }
    }
    return true;
  });

  nix::initNix();
  nix::initGC();
  nix::flake::initLib(nix::flakeSettings);

  myArgs.parseCmdline(nix::argvToStrings(argc, argv));

  nix::settings.readOnlyMode = true;
  auto store = nix::openStore();

  auto evalStore = myArgs.evalStoreUrl
    ? nix::openStore(*myArgs.evalStoreUrl)
    : nix::openStore();
  auto state = nix::make_ref<nix::EvalState>(
      myArgs.lookupPath,
      evalStore,
      nix::fetchSettings,
      nix::evalSettings
    );

  if (config2Expr.empty()) {
    std::atexit([] () { (void)!system(std::format("git worktree remove -f {}", workTree.string()).c_str()); });
    std::filesystem::path tmpDir = "tmp";
    workTree = tmpDir;
    auto exitCode = system(std::format("git worktree add --quiet {}", workTree.string()).c_str());
    assert(exitCode == 0);
  }

  auto flake = config1Expr.substr(0, 2) == ".#";
  if (flake) {
    auto flakePath1 = config1Expr.substr(0, config1Expr.find("#"));
    assert(flakePath1 == ".");
    auto flakeAttr = config1Expr.substr(config1Expr.find("#") + 1);
    config1Expr = std::format("let inherit ((builtins.getFlake (toString ./{})).{}) config; in {{ inherit config; system = config.system.build.toplevel; }}", flakePath1, flakeAttr);

    if (config2Expr.empty()) config2Expr = std::format("let inherit ((builtins.getFlake (toString ./{})).{}) config; in {{ inherit config; system = config.system.build.toplevel; }}", workTree.string(), flakeAttr);
  } else {
    if (config2Expr.empty()) config2Expr = std::format("./{}/{}", workTree.string(), config1Expr);
    if (!expr) {
      config1Expr = "import <nixpkgs/nixos> { configuration = (" + config1Expr + "); }";
      config2Expr = "import <nixpkgs/nixos> { configuration = (" + config2Expr + "); }";
    }
  }

  config1Expr = "let inherit (" + config1Expr + ") config system; in builtins.seq system config";
  config2Expr = "let inherit (" + config2Expr + ") config system; in builtins.seq system config";

  Value config1 = parseAndEval(*state, config1Expr, ".");
  Value config2 = parseAndEval(*state, config2Expr, workTree.empty() ? "." : workTree);

  Context ctx{*state, *myArgs.getAutoArgs(*state), workTree.empty() ? config1 : config2, workTree.empty() ? config2 : config1};

  printDiff(ctx);

  ctx.state.maybePrintStats();

  return 0;
}
