src = [
  'main.cc',
]

cc = meson.get_compiler('cpp')

executable('nixos-diff', src,
           dependencies : [
             boost_dep,
             dtl_dep,
             nix_cmd_dep,
             nix_expr_dep,
             nix_flake_dep,
             nix_main_dep,
             nix_store_dep,
             nix_util_dep,
             nlohmann_json_dep,
             threads_dep
           ],
           install: true,
           cpp_args: ['-std=c++2a', '--include', 'nix/config.h'])
