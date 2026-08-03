#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: the arity -1 entry points in ox.c read *argv without checking
# argc first.
#
# Ox.load, Ox.load_file, Ox.dump, Ox.to_xml and Ox.to_file are all defined with
# arity -1 and went straight to *argv, so calling one with too few arguments
# read the slot an argument would have occupied. What is in that slot is
# whatever the caller's dispatch left there, and it is not the same thing twice:
#
#   Ox.load                    read a Module
#   Ox.load from an includer   read an Object
#   Ox.public_send(:load)      read a BasicObject
#   Ox.method(:load).call      read a Method
#
# None of them crashed, since the slot is mapped, and each was rejected a step
# later by Check_Type or by the dumper. The risk is the value that is not
# rejected: a String there would have been parsed as if the caller had passed
# it. Ox.sax_parse and Ox.sax_html already checked, as does every arity -1
# method in builder.c, so this brings the rest into line.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'tmpdir'
require 'ox'

class ArgcTest < ::Test::Unit::TestCase
  # Every call that used to reach past its arguments, with the message it should
  # now give.
  SHORT_CALLS = {
    'Ox.load' => [-> { Ox.load }, 'missing XML string'],
    'Ox.load_file' => [-> { Ox.load_file }, 'missing file path'],
    'Ox.dump' => [-> { Ox.dump }, 'missing object to dump'],
    'Ox.to_xml' => [-> { Ox.to_xml }, 'missing object to dump'],
    'Ox.to_file' => [-> { Ox.to_file }, 'missing file path or object to write'],
    'Ox.to_file(path)' => [-> { Ox.to_file('/dev/null') }, 'missing file path or object to write']
  }.freeze

  def test_a_short_call_raises_with_a_message
    SHORT_CALLS.each do |where, (call, message)|
      e = assert_raise(Ox::ArgError, where) { call.call }
      assert_equal(message, e.message, where)
    end
  end

  # The slot held something different on each of these paths before the check,
  # so all of them have to end up at the same answer now.
  def test_every_dispatch_path_agrees
    [-> { Ox.load },
     -> { Ox.send(:load) },
     -> { Ox.public_send(:load) },
     -> { Ox.method(:load).call },
     -> { Ox.load(*[]) },
     -> { [nil].map { Ox.load }.first }].each_with_index do |call, i|
      e = assert_raise(Ox::ArgError, "path #{i}") { call.call }
      assert_equal('missing XML string', e.message, "path #{i}")
    end
  end

  # rb_define_module_function also defines a private instance method, so an
  # includer reaches the same code with a different self. That changed what the
  # unchecked read returned.
  class Includer
    include Ox

    def short_load
      load
    end

    def short_dump
      dump
    end
  end

  def test_an_includer_gets_the_same_answer
    assert_raise(Ox::ArgError) { Includer.new.short_load }
    assert_raise(Ox::ArgError) { Includer.new.short_dump }
  end

  # sax_parse and sax_html were already checked and keep their own class and
  # wording, which this change deliberately leaves alone.
  def test_the_sax_entry_points_are_unchanged
    e = assert_raise(Ox::ParseError) { Ox.sax_parse }
    assert_equal("Wrong number of arguments to sax_parse.\n", e.message)
    e = assert_raise(Ox::ParseError) { Ox.sax_html }
    assert_equal("Wrong number of arguments to sax_html.\n", e.message)
  end

  # Only the lower bound is checked, which is what sax_parse does and what every
  # one of these has always done: an extra argument is ignored, not refused.
  def test_extra_arguments_are_still_ignored
    assert_equal("<s>s</s>\n", Ox.dump('s', {}, :extra))
    assert_instance_of(Ox::Element, Ox.load('<r/>', {}, :extra))
  end

  def test_the_calls_still_work_with_the_arguments_they_want
    assert_equal("<s>s</s>\n", Ox.dump('s'))
    assert_equal("<s>s</s>\n", Ox.to_xml('s'))
    assert_instance_of(Ox::Element, Ox.load('<r/>'))
    assert_instance_of(Ox::Element, Ox.load('<r/>', mode: :generic))

    Dir.mktmpdir('ox-argc') do |dir|
      path = File.join(dir, 'doc.xml')
      Ox.to_file(path, Ox::Element.new('r'))
      back = Ox.load_file(path)
      assert_instance_of(Ox::Element, back)
      assert_equal('r', back.value)
    end
  end

  def test_ox_arg_error_is_an_ox_error
    assert_operator(Ox::ArgError, :<, Ox::Error)
    assert_operator(Ox::ArgError, :<, StandardError)
  end
end
