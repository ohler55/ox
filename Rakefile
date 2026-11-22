# frozen_string_literal: true

require 'bundler/gem_tasks'
require 'rake/extensiontask'
require 'rubygems/package_task'
require 'rake/testtask'

# Platforms that get a precompiled native gem, published alongside the source
# gem so most users install Ox without a C toolchain. Each one is built inside
# a rake-compiler-dock container; see the gem:native tasks below and
# .github/workflows/precompiled.yml.
#
# x64-mingw-ucrt and aarch64-mingw-ucrt are the RubyInstaller platforms for
# Windows on x64 (Ruby 3.1 and later) and ARM64 (Ruby 3.4 and later). The
# *-linux-gnu and *-linux-musl gems are built separately because the
# rake-compiler-dock docs say not to mix a bare *-linux gem with *-linux-musl.
#
# The 32-bit glibc gems (arm-linux-gnu, x86-linux-gnu) are built against a Ruby
# with a 32-bit time_t but can be installed on one built with a 64-bit time_t,
# as Debian 13 and Ubuntu 24.04 do on 32-bit ARM. That is safe because Ox does
# not pass time_t to Ruby when it is built against a 32-bit one; see
# ext/ox/time_conv.h.
NATIVE_PLATFORMS = [
  'aarch64-linux-gnu',
  'aarch64-linux-musl',
  'aarch64-mingw-ucrt',
  'arm-linux-gnu',
  'arm-linux-musl',
  'arm64-darwin',
  'x64-mingw-ucrt',
  'x86-linux-gnu',
  'x86-linux-musl',
  'x86_64-darwin',
  'x86_64-linux-gnu',
  'x86_64-linux-musl'
].freeze

# Ruby minor versions compiled into each native gem. Rubies outside this list
# fall back to the source gem and compile it as before.
NATIVE_RUBY_VERSIONS = ['4.0', '3.4', '3.3', '3.2', '3.1'].freeze

# Binaries rake-compiler-dock leaves unstripped, and the strip that fixes them.
# Its cross rubies are linked with -s everywhere but Windows ARM64 and macOS, and
# extensions inherit that, so those two still carry symbols: DWARF debug info
# and a symbol table on Windows ARM64, which is about three quarters of each DLL,
# and local symbols on macOS, where the container's strip re-signs the binary.
NATIVE_STRIP = {
  'aarch64-mingw-ucrt' => 'aarch64-w64-mingw32-strip --strip-all',
  'arm64-darwin' => 'aarch64-apple-darwin-strip -x',
  'x86_64-darwin' => 'x86_64-apple-darwin-strip -x'
}.freeze

# Platforms with fewer Rubies: RubyInstaller for Windows on ARM64, and so
# rake-compiler-dock's cross rubies for it, start at Ruby 3.4.
NATIVE_RUBY_VERSIONS_FOR = Hash.new(NATIVE_RUBY_VERSIONS).merge(
  'aarch64-mingw-ucrt' => ['4.0', '3.4']
).freeze

GEMSPEC = Gem::Specification.load('ox.gemspec')

# Defines the gem task that rake-compiler hooks to package native gems into pkg/.
Gem::PackageTask.new(GEMSPEC).define

Rake::ExtensionTask.new('ox', GEMSPEC) do |ext|
  ext.lib_dir = 'lib/ox'
  ext.cross_compile = true
  ext.cross_platform = NATIVE_PLATFORMS
  ext.cross_compiling do |spec|
    # The native gem ships compiled binaries, so nothing under ext/ is needed:
    # not the C sources, and not extconf.rb, as a native gem has no extensions
    # to build. lib/ is then the only load path.
    spec.files.reject! { |path| path.start_with?('ext/') }
    spec.require_paths = ['lib']
  end
end

# Strip each binary in NATIVE_STRIP as soon as it is compiled, so every copy
# made from it on the way into the native gem is stripped too. Only inside the
# rake-compiler-dock container (which sets RCD_HOST_RUBY_PLATFORM), where the
# strip tools are: a plain rake compile on, say, Windows ARM64 defines the same
# task for its own platform and must be left alone.
if ENV['RCD_HOST_RUBY_PLATFORM']
  Rake::Task.tasks.each do |task|
    platform = task.name[%r{\Atmp/([^/]+)/ox/\d+\.\d+\.\d+/ox\.(?:so|bundle)\z}, 1]
    strip = NATIVE_STRIP[platform] or next

    task.enhance { sh "#{strip} #{task.name}" }
  end
end

namespace :gem do
  namespace :native do
    NATIVE_PLATFORMS.each do |platform|
      desc "Build the precompiled #{platform} gem with rake-compiler-dock"
      task platform do
        require 'rake_compiler_dock'
        RakeCompilerDock.set_ruby_cc_version(*NATIVE_RUBY_VERSIONS_FOR[platform])
        # The container image ships a patched rake-compiler and the cross
        # rubies, so this runs plain rake rather than bundle exec.
        RakeCompilerDock.sh("rake native:#{platform} gem RUBY_CC_VERSION=#{ENV.fetch('RUBY_CC_VERSION')}",
                            platform: platform)
      end
    end
  end

  desc 'Build the precompiled gems for every native platform'
  task native: NATIVE_PLATFORMS.map { |platform| "gem:native:#{platform}" }
end

# test_all invokes this. Without it Rake::Task['test'] silently resolved to a
# synthesized file task for the test/ directory, so everything below never ran.
#
# tests.rb and sax/sax_test.rb are excluded because test_all runs them
# separately through run(), which adds the ASAN preload when OX_ASAN is set.
# cache_test.rb / cache8_test.rb call Ox.cache_test / Ox.cache8_test, C
# self-tests only compiled into a debug build, and smart_test.rb runs
# opts.parse(ARGV) at load and exits on the test runner's -v flag.
Rake::TestTask.new(:test) do |t|
  t.test_files = FileList['test/**/*_test.rb']
                 .exclude('test/cache_test.rb',
                          'test/cache8_test.rb',
                          'test/sax/smart_test.rb',
                          'test/sax/sax_test.rb')
  t.verbose = true
end

if RUBY_PLATFORM.include?('linux')
  begin
    require 'ruby_memcheck'

    RubyMemcheck.config(
      binary_name: 'ox',
      # Valgrind and YJIT interfere with each other, adding noise and slowdown,
      # so keep YJIT disabled while running under Valgrind.
      ruby: "#{FileUtils::RUBY} --disable-yjit"
    )

    # Every *_test.rb in test/, plus tests.rb (which does not match that glob).
    # Excluded: cache_test.rb / cache8_test.rb call Ox.cache_test / Ox.cache8_test,
    # C self-tests only compiled into a debug build; smart_test.rb runs
    # opts.parse(ARGV) at load and exits on the test runner's -v flag. The two
    # fork-based tests in tests.rb are skipped under Valgrind via OX_SKIP_FORK_TESTS
    # (a forked child stays under Valgrind and only adds noise).
    memcheck_test_files =
      FileList['test/**/*_test.rb']
      .exclude('test/cache_test.rb', 'test/cache8_test.rb', 'test/sax/smart_test.rb') + ['test/tests.rb']

    namespace :test do
      desc 'Fail early with a clear message when Valgrind is not installed'
      task :check_valgrind do
        unless system('command -v valgrind > /dev/null 2>&1')
          abort("\nValgrind is required for `rake test:valgrind` but was not found.\n" \
                "Install it first (Linux only), e.g. `sudo apt-get install valgrind`.\n")
        end
        # Inherited by the Ruby child that Valgrind traces; see the note above.
        ENV['OX_SKIP_FORK_TESTS'] = '1'
      end

      RubyMemcheck::TestTask.new(valgrind: [:check_valgrind, :compile]) do |t|
        t.test_files = memcheck_test_files
        t.verbose    = true
      end
    end
  rescue LoadError
    # ruby_memcheck is an optional, Linux-only development dependency. If it is
    # not installed just skip defining the task instead of breaking the Rakefile.
  end
end

def run(command)
  if ENV['OX_ASAN']
    @ld_preload ||= `gcc -print-file-name=libasan.so`.strip
    command = "LD_PRELOAD=#{@ld_preload} ASAN_OPTIONS=detect_leaks=0 #{command}"
  end
  system command
end

task test_all: [:clean, :compile] do
  $stdout.flush
  exitcode = 0
  status = true

  %w[test/tests.rb test/sax/sax_test.rb].each do |test|
    cmds = "bundle exec ruby #{test} -v"

    $stdout.syswrite "\n#{'#' * 90}\n#{cmds}\n"
    Bundler.with_original_env do
      status = status && run(cmds)
    end
  end
  exitcode = 1 unless status

  unless ENV['OX_ASAN']
    Rake::Task['test'].invoke
  else
    run('rake test')
  end
  exit(1) if exitcode == 1
end

task default: :test_all
