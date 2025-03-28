%title: nixos-diff - differences in NixOS configurations
%author: aforemny
%date: 2025-04-25

-> nixos-diff - differences in NixOS configurations <-
======================================================

-> Nix User Group Hannover Braunschweig Wolfsburg <-
----------------------------------------------------

-> Hannover, 25.04.2025 <-
----------------------

-> Alexander Foremny <aforemny@posteo.de> <-
--------------------------------------------
---

# motivation

- [nixos-hardware -> nixos-facter: a no-op?](https://discourse.nixos.org/t/comparing-module-system-configurations)

```diff
 {
   imports = [
-    "${nixos-hardware}/lenovo/thinkpad/x1-extreme/gen3"
+    "${nixos-facter}/modules/nixos/facter.nix"
+    { config.facter.reportPath = ./facter.json; }
   ];
   config = {
```
^

## (aside: nixos-facter)

- hardware-specific configuration based on *hardware parts* instead of laptop
  model

---

# current approaches

- [Gabriella439/nix-diff](https://github.com/Gabriella439/nix-diff)
  - diffs *derivations*
- [khumba/nvd](https://git.sr.ht/~khumba/nvd)
  - diffs derivations
- [nixos-option](https://github.com/NixOS/nixpkgs/tree/c468bf77987cb47744ddd36ae217e1232cff24dc/pkgs/by-name/ni/nixos-option)
  - no diffing
  - recursive evaluation of config impractical

---

# history

- [Ocean Sprint 2025](https://oceansprint.org/reports/2025/)

- to *fully evaluate config in Nix*, possible but:
  - needs stronger `builtins.tryEval`
  - *needs arbitrary exclusion of package sets (infinite growth)*

- my take-away: config not meant to be evaluated fully

---

# another appraoch

[by rhendric](https://discourse.nixos.org/t/comparing-module-system-configurations/59654/8)
```console
nix-instantiate --xml --eval --expr '
  let
    inherit (import <nixpkgs/nixos> {}) config system;
  in
  builtins.seq system config;
'
```

- evaluates `system`, prints `config`
- *unevaluated parts of* `config` *are not relevant for realizing* `system`

^

- (XML output also deals with infinite growth)

---

# nixos-diff - api

*(legacy) nixos configuration*

```console
nixos-diff ./config1.nix ./config2.nix
```

*(legacy) nixos configuration (Git aware)*

```console
nixos-diff ./config1.nix
```

*(flake) nixos configuration*

```console
nixos-diff .#nixosConfigurations.machine1 .#nixosConfigurations.machine2
```

*(flake) nixos configuration (Git aware)*

```console
nixos-diff .#nixosConfigurations.machine1
```

---

# test case: smoke test

```console
$ diff -u ./config1.nix ./config2.nix
 {
-  networking.hostName = "nixos";
+  networking.hostName = "machine";
 }
```

```console
$ \time nixos-diff ./config1.nix ./config2.nix
11.49 user 2.24 system 0:13.87 elapsed 99% CPU (1473880 maxresident)k
```

*output (exerpt!)*
```
-environment.etc.hostname.text = "nixos\n";
+environment.etc.hostname.text = "machine\n";
-networking.hostName = "nixos";
+networking.hostName = "machine";
```

---

# test case: nixos-hardware

```console
$ diff -u ./config1.nix ./config2.nix
 {
   imports = [
-    "${nixos-hardware}/lenovo/thinkpad/x1-extreme/gen3"
+    "${nixos-facter}/modules/nixos/facter.nix"
+    { config.facter.reportPath = ./facter.json; }
   ];
   config = {
```

```console
$ \time nixos-diff ./config1.nix ./config2.nix >output.diff
9.33 user 0.77 system 0:10.19 elapsed 99% CPU (1724732 maxresident)k
```

*output (excerpt!)*
```
-systemd.services.resolvconf.enable = true;
+systemd.services.bluetooth.enable = true;
-systemd.services.dhcpcd.enable = true;
+systemd.services.fprintd.enable = true;
-systemd.services.tlp.enable = true;
-systemd.services.cpufreq.enable = false;
-systemd.services.systemd-rfkill.enable = false;
-systemd.services.tlp-sleep.enable = true;
+systemd.services.systemd-networkd.enable = true;
+systemd.services.systemd-networkd-wait-online.enable = true;
+systemd.services."systemd-networkd-wait-online@".enable = true;
+systemd.services.systemd-resolved.enable = true;
-systemd.services.network-setup.enable = true;
-systemd.services.trackpoint.enable = true;
```

---

# a word of caution

- this is an *extremely early* prototype
- on most real-world configs, I
  - expect this to fail
  - expect this to take longer (not more than 5 minutes?)
  - expect this to take more RAM (not more than 6GiB?)

---

# contribute

- [aforemny/nixos-diff](https://github.com/aforemny/nixos-diff)

- not a lot of history:
```console
$ git log --oneline
4420a7b init
```

- not a lot of code:
```console
$ cloc --vcs=git --include-lang C++
-------------------------------------------------------------------------------
Language                     files          blank        comment           code
-------------------------------------------------------------------------------
C++                              1             41             11            289
-------------------------------------------------------------------------------
```
