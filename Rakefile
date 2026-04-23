# frozen_string_literal: true

require 'bundler/gem_tasks'
require 'rake/extensiontask'
require 'rake/testtask'

Rake::ExtensionTask.new('ox') do |ext|
  ext.lib_dir = 'lib/ox'
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
