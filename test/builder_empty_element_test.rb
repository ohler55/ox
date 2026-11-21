#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: Ox::Builder leaked the name of an empty element.
#
# builder_element() strdup()s a name that does not fit the 64-byte inline
# buffer, and pop() freed that copy only when the element had children. An
# element closed with "/>" kept the copy, and its stack slot was overwritten by
# the next element, so every empty long-named element leaked its name.
#
# The leak itself is caught by rake test:valgrind; this keeps the path
# exercised and checks the output is unchanged.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class BuilderEmptyElementTest < ::Test::Unit::TestCase
  LONG = 'n' * 100

  def test_empty_long_named_element_with_pop
    200.times do
      b = Ox::Builder.new(indent: 0)
      b.element(LONG)
      b.pop
      assert_equal("<#{LONG}/>\n", b.to_s)
    end
  end

  def test_empty_long_named_element_with_block
    200.times do
      xml = Ox::Builder.new(indent: 0) do |b|
        b.element('top') do
          b.element(LONG, 'a' => '1') { nil }
        end
      end
      assert_equal(%(<top><#{LONG} a="1"/></top>\n), xml)
    end
  end

  def test_empty_long_named_element_closed_by_close
    200.times do
      b = Ox::Builder.new(indent: 0)
      3.times { |i| b.element("#{LONG}#{i}") }
      b.close
      assert_equal("<#{LONG}0><#{LONG}1><#{LONG}2/></#{LONG}1></#{LONG}0>\n", b.to_s)
    end
  end

  def test_slot_reused_after_empty_long_named_element
    b = Ox::Builder.new(indent: 0)
    b.element(LONG)
    b.pop
    b.element('short')
    b.text('x')
    b.pop
    b.close
    assert_equal("<#{LONG}/><short>x</short>\n", b.to_s)
    assert_nothing_raised { GC.start(full_mark: true, immediate_sweep: true) }
  end
end
