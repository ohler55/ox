#!/usr/bin/env rake

require 'rake'
require 'rake/extensiontask'
require 'rake/testtask'
require 'yard'

task :default => :build

task :build => [:clean, :compile]

YARD::Rake::YardocTask.new(:doc) do |y|
  # use defaults
end

Rake::ExtensionTask.new(:compile) do |ext|
  ext.name = 'ox'
  ext.ext_dir = 'ext/ox'
  #  ext.lib_dir = 'lib/ox'
  ext.lib_dir = 'ext/ox'
end

Rake::TestTask.new(:test) do |t|
  t.test_files = FileList['test/func.rb', 'test/sax_test.rb']
  t.verbose = true
end
