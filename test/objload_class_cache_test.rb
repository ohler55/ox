#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: object mode registered the cache slot of each class, but
# slot_cache_get() moves classes between slots, so a removed class was freed
# or a compacted one was moved while the cache still returned it.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class ObjLoadClassCacheTest < ::Test::Unit::TestCase
  ROUNDS = 20

  def test_class_looked_up_twice_survives_gc
    names = define_classes('ObjLoadCacheTwice', ROUNDS)
    names.each { |n| 2.times { load_object(n) } }
    assert_classes_survive(names)
  end

  def test_class_moved_by_another_name_survives_gc
    names = define_classes('ObjLoadCacheMovedA', ROUNDS)
    others = define_classes('ObjLoadCacheMovedB', ROUNDS)
    names.zip(others).each do |n, o|
      load_object(n)
      load_object(o)
    end
    assert_classes_survive(names)
  end

  def test_class_survives_compaction
    omit('needs GC.verify_compaction_references with expand_heap') unless RUBY_ENGINE == 'ruby' && RUBY_VERSION >= '3.2'
    names = define_classes('ObjLoadCacheCompact', 3)
    names.each do |n|
      2.times { load_object(n) }
      begin
        GC.verify_compaction_references(expand_heap: true, toward: :empty)
      rescue NotImplementedError
        omit('GC compaction is not supported here')
      end
      assert_equal(n, load_object(n).class.name)
    end
  end

  private

  def define_classes(prefix, count)
    Array.new(count) do |i|
      name = "#{prefix}#{i}"
      Object.const_set(name, Class.new)
      name
    end
  end

  def load_object(name)
    Ox.load(%(<o c="#{name}"/>), mode: :object)
  end

  def assert_classes_survive(names)
    names.each { |n| Object.send(:remove_const, n) }
    GC.start(full_mark: true, immediate_sweep: true)
    names.each { |n| assert_equal(n, load_object(n).class.name) }
  end
end
