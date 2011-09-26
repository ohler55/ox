#!/usr/bin/env rake

require 'rake'
require 'rake/testtask'
require 'yard'

task :default => :build

task :build => :test

YARD::Rake::YardocTask.new(:doc) do |y|
  # use defaults
end

namespace :compile do
  require 'rake/extensiontask'
  Rake::ExtensionTask.new do |ext|
    ext.name = 'ox'
    ext.ext_dir = 'ext/ox'
    #  ext.lib_dir = 'lib/ox'
    ext.lib_dir = 'ext/ox'
  end
end

Rake::TestTask.new(:test) do |t|
  t.test_files = FileList['test/func.rb', 'test/sax_test.rb']
  t.verbose = true
end
