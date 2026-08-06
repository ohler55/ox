#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: an element or attribute name was written with no check that
# it could be a name, so a name could end itself and start something else.
#
#   e = Ox::Element.new('r')
#   e[%q{a="1" b}] = '2'
#   Ox.dump(e)                       #=> "\n<r a=\"1\" b=\"2\"/>\n"
#   Ox.parse(Ox.dump(e)).attributes  #=> {a: "1", b: "2"}    one in, two out
#
# Both writers did it and neither said anything. Ox::Builder ran a name through
# xml_element_chars, which escapes '<', '>' and '&' but not '"', so it stopped
# the element name injection and not the attribute one; the DOM dumper writes a
# name with fill_value and escaped nothing at all.
#
#   name       Ox.dump elem   Builder elem   Ox.dump attr   Builder attr
#   a<b        injects        &lt;           injects        &lt;
#   a="1" b    -              -              INJECTS        INJECTS
#   a b        <a b/>         <a b/>         <r a b="1"/>   <r a b="1"/>
#   a\0b       document cut   raised         raised         raised
#   a\x01b     written raw    raised         written raw    raised
#
# Escaping is not the fix even where a table has an escape: a name is not a
# place a character reference is expanded, so &lt; in a name reads back as four
# more characters rather than as the one that was asked for.
#
# Issue #469 settled on the narrow rule, so what is refused is only what could
# end the name where it is written -- a byte at or below a space, '<', '>',
# '&', '/', '=', and '"' in an attribute name. A name that is odd but reads
# back whole, '1abc' or 'a-b' or an element name holding a '"', still goes out.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class NameCharsTest < ::Test::Unit::TestCase
  # Every way a name reaches the buffer, keyed by what it is a name of. The
  # value takes the name and returns the document.
  ELEMENT = {
    'Ox.dump element'            => ->(n) { Ox.dump(Ox::Element.new(n)) },
    'Builder#element'            => ->(n) { Ox::Builder.new(indent: -1) { |b| b.element(n) } },
    'Builder#element symbol'     => ->(n) { Ox::Builder.new(indent: -1) { |b| b.element(n.to_sym) } },
    'Builder#void_element'       => ->(n) { Ox::Builder.new(indent: -1) { |b| b.void_element(n) } },
  }.freeze

  ATTRIBUTE = {
    'Ox.dump attribute'          => ->(n) { e = Ox::Element.new('r'); e[n] = '1'; Ox.dump(e) },
    'Ox.dump attribute symbol'   => ->(n) { e = Ox::Element.new('r'); e[n.to_sym] = '1'; Ox.dump(e) },
    'Ox.dump instruct attribute' => lambda { |n|
      i = Ox::Instruct.new('xml')
      i[n] = '1'
      d = Ox::Document.new
      d << i
      d << Ox::Element.new('r')
      Ox.dump(d)
    },
    'Builder#element attribute'  => ->(n) { Ox::Builder.new(indent: -1) { |b| b.element('r', n => '1') } },
    'Builder attribute symbol'   => ->(n) { Ox::Builder.new(indent: -1) { |b| b.element('r', n.to_sym => '1') } },
  }.freeze

  WRITERS = ELEMENT.merge(ATTRIBUTE).freeze

  # Ends the name in every position it is written in.
  BOTH = ['<', '>', '&', '/', '=', ' ', "\t", "\n", "\r"].freeze

  # An attribute value is written in double quotes, so only there.
  ATTR_ONLY = ['"'].freeze

  def refused?
    yield
    false
  rescue Ox::SyntaxError
    true
  end

  def test_every_writer_refuses_a_character_that_ends_the_name
    BOTH.each do |c|
      WRITERS.each do |where, w|
        assert_raise(Ox::SyntaxError, "#{where} #{c.inspect}") { w.call("a#{c}b") }
      end
    end
  end

  def test_only_an_attribute_name_refuses_the_quote
    ATTR_ONLY.each do |c|
      ATTRIBUTE.each do |where, w|
        assert_raise(Ox::SyntaxError, "#{where} #{c.inspect}") { w.call("a#{c}b") }
      end
      ELEMENT.each do |where, w|
        assert_nothing_raised(where) { w.call("a#{c}b") }
      end
    end
  end

  # The two writers disagreed here and it was the attribute half that injected,
  # so state it as the document that came back rather than as a raise.
  def test_the_attribute_injection_is_closed_in_both_writers
    name = %q{a="1" b}
    assert_raise(Ox::SyntaxError) do
      e = Ox::Element.new('r')
      e[name] = '2'
      Ox.dump(e)
    end
    assert_raise(Ox::SyntaxError) { Ox::Builder.new(indent: -1) { |b| b.element('r', name => '2') } }
  end

  def test_the_element_injection_is_closed_in_both_writers
    name = 'a<b/><injected'
    assert_raise(Ox::SyntaxError) { Ox.dump(Ox::Element.new(name)) }
    assert_raise(Ox::SyntaxError) { Ox::Builder.new(indent: -1) { |b| b.element(name) } }
  end

  # A NUL used to cut the whole document short in the one writer that let it
  # through, since rb_str_new2() stops at it.
  def test_a_nul_no_longer_cuts_the_document
    assert_raise(Ox::SyntaxError) { Ox.dump(Ox::Element.new("a\0b")) }
  end

  # Below a space the byte is one XML can not hold at all, which is a different
  # complaint from a name it could hold but can not use, so the message the
  # value writers already raise with is kept.
  def test_the_whole_control_range_raises_everywhere
    (0..0x1f).each do |b|
      c = b.chr
      WRITERS.each do |where, w|
        e = assert_raise(Ox::SyntaxError, "#{where} #{format('%02x', b)}") { w.call("a#{c}b") }
        want = if ["\t", "\n", "\r"].include?(c)
                 kind = where.include?('attribute') ? 'an attribute' : 'an element'
                 format("'\\#x%02x' can not be used in %s name.", b, kind)
               else
                 format("'\\#x%02x' is not a valid XML character.", b)
               end
        assert_equal(want, e.message, where)
      end
    end
  end

  # The scan reads a 256 entry table, where a wrong column would be silent. So
  # state the rule again here, independently of the table, and hold every byte
  # in both positions against it.
  def test_the_rule_holds_for_every_byte
    (0..255).each do |b|
      name = "a#{b.chr}b".dup.force_encoding('ASCII-8BIT')
      elem = b <= 0x20 || ['<', '>', '&', '/', '='].include?(b.chr)
      attr = elem || b.chr == '"'
      assert_equal(elem, refused? { Ox.dump(Ox::Element.new(name)) }, format('element %02x', b))
      assert_equal(attr, refused? { e = Ox::Element.new('r'); e[name] = '1'; Ox.dump(e) },
                   format('attribute %02x', b))
    end
  end

  def test_the_message_names_the_character_and_the_kind
    e = assert_raise(Ox::SyntaxError) { Ox.dump(Ox::Element.new('a<b')) }
    assert_equal("'<' can not be used in an element name.", e.message)

    e = assert_raise(Ox::SyntaxError) { Ox::Builder.new(indent: -1) { |b| b.element('r', 'a=b' => '1') } }
    assert_equal("'=' can not be used in an attribute name.", e.message)

    e = assert_raise(Ox::SyntaxError) { Ox::Builder.new(indent: -1) { |b| b.element('a b') } }
    assert_equal("'\\#x20' can not be used in an element name.", e.message)
  end

  # The narrow rule stops here on purpose: these are not names XML would take,
  # but they read back as the one name that was asked for, so refusing them
  # would break callers for no gain.
  def test_a_name_that_reads_back_whole_still_goes_out
    ['1abc', 'a-b', 'a.b', 'a:b', 'a!b', "a'b", '日本語'].each do |n|
      ELEMENT.each do |where, w|
        assert_nothing_raised("#{where} #{n.inspect}") { w.call(n) }
      end
      ATTRIBUTE.each do |where, w|
        assert_nothing_raised("#{where} #{n.inspect}") { w.call(n) }
      end
    end
    e = Ox::Element.new('1abc')
    e['a-b'] = '1'
    assert_equal("\n<1abc a-b=\"1\"/>\n".b, Ox.dump(e))
    assert_equal('<1abc a-b="1"/>', Ox::Builder.new(indent: -1) { |b| b.element('1abc', 'a-b' => '1') })
  end

  def test_a_valid_document_is_unchanged
    e = Ox::Element.new('top')
    e['name'] = 'a<b'
    e << (c = Ox::Element.new('kid-1:ns'))
    c << 'text & more'
    assert_equal("\n<top name=\"a&lt;b\">\n  <kid-1:ns>text &amp; more</kid-1:ns>\n</top>\n".b, Ox.dump(e))
    assert_equal('<top name="a&lt;b"><kid-1:ns>text &amp; more</kid-1:ns></top>',
                 Ox::Builder.new(indent: -1) { |b|
                   b.element('top', 'name' => 'a<b') { b.element('kid-1:ns') { b.text('text & more') } }
                 })
  end

  # The check has to run before the '<', or a rescued raise leaves an element
  # started that no later call can finish. Same property as issue #465.
  def test_a_rescued_element_raise_leaves_nothing_behind
    b = Ox::Builder.new(indent: -1)
    b.element('r')
    b.element('c', 'ok' => '1')
    assert_raise(Ox::SyntaxError) { b.element('bad name') }
    assert_raise(Ox::SyntaxError) { b.void_element('bad<name') }
    b.text('t')
    b.pop
    b.pop
    assert_equal('<r><c ok="1">t</c></r>', b.to_s)
  end

  # An attribute is written whole or not at all and the element it belongs to is
  # left as it was, which is what the value check already promised.
  def test_a_rescued_attribute_raise_writes_no_part_of_it
    bad = Ox::Builder.new(indent: -1)
    bad.element('r')
    assert_raise(Ox::SyntaxError) { bad.element('c', 'ok' => '1', 'x=y' => '2') }

    val = Ox::Builder.new(indent: -1)
    val.element('r')
    assert_raise(Ox::SyntaxError) { val.element('c', 'ok' => '1', 'z' => "\x01") }

    assert_equal(val.to_s, bad.to_s)
    assert_equal('<r><c ok="1"', bad.to_s)
  end

  # A processing instruction target is the Instruct's value, and issue #460
  # settled that a value closing its own construct is the caller's business.
  # Both writers leave it alone, so pin that they agree.
  def test_an_instruct_target_is_left_alone
    d = Ox::Document.new
    d << Ox::Instruct.new('a b')
    d << Ox::Element.new('r')
    assert_equal("<?a b?>\n<r/>\n".b, Ox.dump(d))
    assert_equal('<?a b?><r/>', Ox::Builder.new(indent: -1) { |b| b.instruct('a b'); b.element('r') })
  end

  # The scan walks the name a byte at a time, so put the bad byte at every
  # offset either side of the 64 byte inline buffer Builder copies a name into.
  def test_the_byte_is_found_at_any_offset
    [1, 7, 8, 9, 62, 63, 64, 65, 200].each do |n|
      name = "#{'a' * n}<b"
      WRITERS.each do |where, w|
        assert_raise(Ox::SyntaxError, "#{where} #{n}") { w.call(name) }
      end
    end
  end
end
