# real_bench/ — where each file comes from

These 50 files are compression-benchmark inputs, not part of mzip. The 28 files copied from public projects keep
their original copyright and license; the license texts they require are in `../LICENSES/`. The other 22 have no
known upstream: most carry clear signs of having been generated for this benchmark (row-numbered fake hashes,
`@example.com` addresses, invalid UUIDs and CSS), and nine are generic configuration templates that code search
could not trace. Their license is recorded as unknown rather than assumed.

Traced on 2026-09-27 against the commit that added the files (d3d99f4, 2026-01-04): 26 files match their
upstream byte for byte at the linked commit; `php_laravel.php` differs by one later two-line change; the JSON
file is a live API response. The README's older real-world tables count 47 files: that is these 50 minus the
three dotfiles, which a shell glob such as `real_bench/*` does not match.

- `apache_log_sample.log` is Elastic's public `apache_logs` sample: a real May 2015 access log of
  semicomplete.com, including real client IP addresses.
- `vscode_main.ts` is VS Code's `src/vs/workbench/browser/workbench.ts`, not `src/main.ts`.
- `sql_schema.sql` (MySQL employees sample database) is CC BY-SA 3.0: credits are in the file's header.

| file | source | license | license text |
|---|---|---|---|
| `apache_log_sample.log` | [elastic/examples (archived)](https://github.com/elastic/examples/blob/master/Common%20Data%20Formats/apache_logs/apache_logs) `Common Data Formats/apache_logs/apache_logs` | Apache-2.0 | ../LICENSE (Apache-2.0) |
| `api_docs.md` | none found — generated for this benchmark (generator fingerprints) | unknown | — |
| `app.log` | none found — generated for this benchmark (generator fingerprints) | unknown | — |
| `bootstrap.css` | [twbs/bootstrap](https://github.com/twbs/bootstrap/blob/25aa8cc0b32f0d1a54be575347e6d84b70b1acd7/dist/css/bootstrap.css) `dist/css/bootstrap.css` | MIT | bootstrap-MIT.txt |
| `clojure_core.clj` | [clojure/clojure](https://github.com/clojure/clojure/blob/8ae9e4f95e2fbbd4ee4ee3c627088c45ab44fa68/src/clj/clojure/core.clj) `src/clj/clojure/core.clj` | EPL-1.0 | clojure-EPL-1.0.txt |
| `contributing.md` | [kubernetes/community](https://github.com/kubernetes/community/blob/37db9dcba459c26e1df10c29268b4e060a950d01/contributors/guide/README.md) `contributors/guide/README.md` | Apache-2.0 | ../LICENSE (Apache-2.0) |
| `cpp_vector.hpp` | [llvm/llvm-project (libc++)](https://github.com/llvm/llvm-project/blob/e9280a1d39af88468ffea9a14fad5bf96d51d6e6/libcxx/include/vector) `libcxx/include/vector` | Apache-2.0 WITH LLVM-exception | llvm-libcxx-Apache-2.0-WITH-LLVM-exception.txt |
| `csharp_list.cs` | [dotnet/runtime](https://github.com/dotnet/runtime/blob/c1bf33e715c336b8d4bdd8b5fa2512a9232d42fa/src/libraries/System.Private.CoreLib/src/System/Collections/Generic/List.cs) `src/libraries/System.Private.CoreLib/src/System/Collections/Generic/List.cs` | MIT | dotnet-runtime-MIT.txt |
| `dashboard.html` | none found — generated for this benchmark (generator fingerprints) | unknown | — |
| `django_models.py` | [django/django](https://github.com/django/django/blob/73c987eb3b6a96300f7238dff32caf53cafe2098/django/db/models/base.py) `django/db/models/base.py` | BSD-3-Clause | django-BSD-3-Clause.txt |
| `docker-compose.yml` | none found — generic template; no upstream found | unknown | — |
| `Dockerfile` | none found — generic template; no upstream found | unknown | — |
| `elixir_genserver.ex` | [elixir-lang/elixir](https://github.com/elixir-lang/elixir/blob/8d8111af07831666edc21f8830033a9b327aaf93/lib/elixir/lib/gen_server.ex) `lib/elixir/lib/gen_server.ex` | Apache-2.0 | ../LICENSE (Apache-2.0) |
| `.env.example` | none found — generic template; no upstream found | unknown | — |
| `events.csv` | none found — generated for this benchmark (generator fingerprints) | unknown | — |
| `flask_app.py` | [pallets/flask](https://github.com/pallets/flask/blob/6a64969009bf1898bd302e6a9633f71942523f7f/src/flask/app.py) `src/flask/app.py` | BSD-3-Clause | flask-BSD-3-Clause.txt |
| `.github_workflows_ci.yml` | none found — generic template; no upstream found | unknown | — |
| `.gitignore` | none found — generic template; no upstream found | unknown | — |
| `go_http.go` | [golang/go](https://github.com/golang/go/blob/65ef314f89d7eb8c1a4937edd075127a00da0ecb/src/net/http/server.go) `src/net/http/server.go` | BSD-3-Clause | go-BSD-3-Clause.txt |
| `handlers.go` | none found — generated for this benchmark (generator fingerprints) | unknown | — |
| `handlers.ts` | none found — generated for this benchmark (generator fingerprints) | unknown | — |
| `java_arraylist.java` | [openjdk/jdk](https://github.com/openjdk/jdk/blob/aeea8497562aabda12f292ad93c9f0f6935cc842/src/java.base/share/classes/java/util/ArrayList.java) `src/java.base/share/classes/java/util/ArrayList.java` | GPL-2.0-only WITH Classpath-exception-2.0 | openjdk-GPL-2.0-with-Classpath-exception.txt |
| `json_github_api.json` | GitHub REST API response `GET /repos/facebook/react`, fetched 2026-01-04 | unknown (GitHub Terms of Service) | — |
| `julia_base.jl` | [JuliaLang/julia](https://github.com/JuliaLang/julia/blob/d1b898de63a14d1a104086393253bbe8e77fbbfe/base/array.jl) `base/array.jl` | MIT | julia-MIT.txt |
| `k8s_deployments.yaml` | none found — generated for this benchmark (generator fingerprints) | unknown | — |
| `kotlin_stdlib.kt` | [JetBrains/kotlin](https://github.com/JetBrains/kotlin/blob/a8b78dcf55a6f09b286f9c6b486a3185036f6380/libraries/stdlib/src/kotlin/collections/Collections.kt) `libraries/stdlib/src/kotlin/collections/Collections.kt` | Apache-2.0 | ../LICENSE (Apache-2.0), kotlin-NOTICE.txt |
| `linux_kernel.c` | [torvalds/linux](https://github.com/torvalds/linux/blob/v6.19-rc3/kernel/sched/core.c) `kernel/sched/core.c` | GPL-2.0-only | GPL-2.0.txt |
| `linux_makefile` | [torvalds/linux](https://github.com/torvalds/linux/blob/v6.19-rc3/Makefile) `Makefile` | GPL-2.0-only | GPL-2.0.txt |
| `lodash.js` | [lodash/lodash](https://github.com/lodash/lodash/blob/19c9251b3631d7cf220b43bc757eb33f1084f117/lodash.js) `lodash.js` | MIT | lodash-MIT.txt |
| `lua_neovim.lua` | [neovim/neovim](https://github.com/neovim/neovim/blob/ed562c296abeac25fc5d81708b9ada09da608772/runtime/lua/vim/lsp.lua) `runtime/lua/vim/lsp.lua` | Apache-2.0 | neovim-LICENSE.txt |
| `Makefile` | none found — generic template; no upstream found | unknown | — |
| `metrics.prom` | none found — generated for this benchmark (generator fingerprints) | unknown | — |
| `models.rs` | none found — generated for this benchmark (generator fingerprints) | unknown | — |
| `nginx_access.log` | none found — generated for this benchmark (generator fingerprints) | unknown | — |
| `php_laravel.php` | [laravel/framework (`master` branch, 13.x-dev)](https://github.com/laravel/framework/blob/f096efedb17701bc03d05dc87f229ff32ca8b9e5/src/Illuminate/Foundation/Application.php) `src/Illuminate/Foundation/Application.php` | MIT | laravel-framework-MIT.txt |
| `readme_large.md` | [sindresorhus/awesome](https://github.com/sindresorhus/awesome/blob/1345b0a348d0abb20189b5154159a7cc08bbd35f/readme.md) `readme.md` | CC0-1.0 | none required (CC0-1.0) |
| `ruby_rails.rb` | [rails/rails](https://github.com/rails/rails/blob/308594b5805e19b920fdbabd35a8b01016ca67c2/activerecord/lib/active_record/base.rb) `activerecord/lib/active_record/base.rb` | MIT | rails-activerecord-MIT.txt |
| `rust_lib.rs` | [rust-lang/rust](https://github.com/rust-lang/rust/blob/ae09be596846d60d14f50399da91ffcb70bfd12e/library/std/src/lib.rs) `library/std/src/lib.rs` | MIT OR Apache-2.0 | rust-COPYRIGHT.txt, rust-LICENSE-MIT.txt |
| `scala_list.scala` | [scala/scala](https://github.com/scala/scala/blob/bdce2137cd737f0ca0dffa3cc3d91ef8f5e92a4a/src/library/scala/collection/immutable/List.scala) `src/library/scala/collection/immutable/List.scala` | Apache-2.0 | ../LICENSE (Apache-2.0), scala-NOTICE.txt |
| `services.py` | none found — generated for this benchmark (generator fingerprints) | unknown | — |
| `sql_schema.sql` | [datacharmer/test_db (MySQL "employees" sample DB)](https://github.com/datacharmer/test_db/blob/fca5b774b7eec12f8fa488fc7129d8be2fc271db/employees.sql) `employees.sql` | CC-BY-SA-3.0 | https://creativecommons.org/licenses/by-sa/3.0/ |
| `styles.css` | none found — generated for this benchmark (generator fingerprints) | unknown | — |
| `swift_stdlib.swift` | [swiftlang/swift](https://github.com/swiftlang/swift/blob/41d1fce6cb8abda7d05b7ea695ab132c1d6ee970/stdlib/public/core/Array.swift) `stdlib/public/core/Array.swift` | Apache-2.0 WITH Swift-exception | swift-Apache-2.0-WITH-runtime-exception.txt |
| `terraform_main.tf` | none found — generic template; no upstream found | unknown | — |
| `users.json` | none found — generated for this benchmark (generator fingerprints) | unknown | — |
| `users_dump.sql` | none found — generated for this benchmark (generator fingerprints) | unknown | — |
| `vscode_main.ts` | [microsoft/vscode](https://github.com/microsoft/vscode/blob/e5e1fca76c2dc15d7d4428d1d417dd3fc05842a9/src/vs/workbench/browser/workbench.ts) `src/vs/workbench/browser/workbench.ts` | MIT | vscode-MIT.txt |
| `webpack.config.js` | none found — generic template; no upstream found | unknown | — |
| `xml_maven.xml` | [apache/maven](https://github.com/apache/maven/blob/29349bb8488fb3fb000bc99c0889c2669ddb4968/pom.xml) `pom.xml` | Apache-2.0 | ../LICENSE (Apache-2.0), maven-NOTICE.txt |
| `zig_std.zig` | [ziglang/zig](https://github.com/ziglang/zig/blob/dec1163fbb892f276179ae74b51007c656157d99/lib/std/mem.zig) `lib/std/mem.zig` | MIT | zig-MIT.txt |
