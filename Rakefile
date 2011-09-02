#!/usr/bin/env rake

require 'rake'
require 'rake/extensiontask'
require 'yard'

task :default => :compile

YARD::Rake::YardocTask.new(:doc) do |y|
  # use defaults
end

Rake::ExtensionTask.new(:compile) do |ext|
  ext.name = 'ox'
  ext.ext_dir = 'ext/ox'
  ext.lib_dir = 'lib/ox'
end
