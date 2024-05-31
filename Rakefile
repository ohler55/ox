# frozen_string_literal: true

require 'bundler/gem_tasks'
require 'rake/extensiontask'
require 'rake/testtask'

Rake::ExtensionTask.new('ox') do |ext|
  ext.lib_dir = 'lib/ox'
end

task test_all: [:clean, :compile] do
  $stdout.flush
  exitcode = 0
  status = 0

  cmds = 'bundle exec ruby test/tests.rb -v'

  $stdout.syswrite "\n#{'#' * 90}\n#{cmds}\n"
  Bundler.with_original_env do
    status = system(cmds)
  end
  exitcode = 1 unless status

  Rake::Task['test'].invoke
  exit(1) if exitcode == 1
end

task default: :test_all
