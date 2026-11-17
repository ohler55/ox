#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: Ox.load, Ox.load_file, Ox.parse and Ox.parse_obj copy a
# document of more than 4096 bytes to the heap and freed the copy only on a
# normal return. A raise out of the parse, such as the NameError for an unknown
# class in object mode, leaked the whole copy, and Ox.load_file also left the
# file open. The memory leak is caught by Valgrind memcheck (rake
# test:valgrind); the open file is checked here where /proc/self/fd exists.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'tempfile'
require 'test/unit'
require 'ox'

class LoadRaiseLeakTest < ::Test::Unit::TestCase
  # Past the 4096 bytes copied on the stack, so the copy is on the heap.
  PAD = ' ' * 8192
  UNKNOWN_CLASS = %(<o c="NoSuchOxLoadRaiseLeakClass"/>#{PAD})

  def assert_still_healthy
    assert_equal({ 'k' => 1 }, Ox.load(Ox.dump({ 'k' => 1 }, mode: :object), mode: :object))
  end

  def test_load_object_mode_raise
    100.times do
      assert_raise(NameError) { Ox.load(UNKNOWN_CLASS, mode: :object) }
    end
    assert_still_healthy
  end

  def test_parse_obj_raise
    100.times do
      assert_raise(NameError) { Ox.parse_obj(UNKNOWN_CLASS) }
    end
    assert_still_healthy
  end

  def test_load_callback_raise
    100.times do
      assert_raise(ArgumentError) do
        Ox.load("<a/>#{PAD}", mode: :generic, element_key_mod: ->(_) { raise ArgumentError })
      end
    end
    assert_still_healthy
  end

  def test_load_file_raise
    Tempfile.create(['ox_load_raise', '.xml']) do |f|
      f.write(UNKNOWN_CLASS)
      f.close
      fd_dir = '/proc/self/fd'
      before = File.directory?(fd_dir) ? Dir.children(fd_dir).size : nil
      100.times do
        assert_raise(NameError) { Ox.load_file(f.path, mode: :object) }
      end
      assert_equal(before, Dir.children(fd_dir).size) unless before.nil?
    end
    assert_still_healthy
  end

  def test_valid_large_documents_unchanged
    xml = %(<a b="1">x</a>#{PAD})
    assert_equal('x', Ox.load(xml, mode: :generic).text)
    assert_equal('x', Ox.parse(xml).text)
    assert_equal('x', Ox.parse_obj(%(<s>x</s>#{PAD})))
    Tempfile.create(['ox_load_raise', '.xml']) do |f|
      f.write(xml)
      f.close
      assert_equal('1', Ox.load_file(f.path, mode: :generic)[:b])
    end
  end
end
