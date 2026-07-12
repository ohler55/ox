# frozen_string_literal: true

require 'bundler/gem_tasks'
require 'rake/extensiontask'
require 'rake/testtask'

Rake::ExtensionTask.new('ox') do |ext|
  ext.lib_dir = 'lib/ox'
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
