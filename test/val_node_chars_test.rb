#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: the value nodes were written without checking what was in
# them.
#
# A comment, a CDATA section, a DOCTYPE and a processing instruction all copy
# their value through instead of escaping it, and none of those writers looked
# at it first. Text and names have always raised on a character XML has no way
# to write, so the two halves of the same class disagreed:
#
#   input "a\0b" / "a\x01b"    Ox::Builder      Ox.dump
#   text                       raise            raise
#   element / attribute name   raise            raise
#   comment                    raise            wrote it
#   doctype                    raise            wrote it
#   cdata                      wrote it         wrote it
#   instruct target            raise            wrote it
#   instruct content / attrs   wrote it         wrote it
#
# A NUL is the case that loses data. Ox.dump returns its buffer with
# rb_str_new2, which stops at the first NUL, so the byte does not merely go out
# invalid - everything after it, including the rest of the document, is gone and
# nothing is raised:
#
#   Ox.dump(Ox::Element.new('r') << Ox::Comment.new("a\0b"))  #=> "\n<r>\n  <!--a"
#
# Escaping is not the answer for these nodes: a character reference is not
# expanded inside a comment, a CDATA section or an instruction, so &#x1; there
# is five more wrong characters rather than one right one. The only thing the
# value can be is a run of Char, so they now refuse anything else, which is what
# every other writer already did.
#
# Ox::Raw is documented as going out untouched and still does.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class ValNodeCharsTest < ::Test::Unit::TestCase
  # Every writer that copies a value through, as a block taking the byte to
  # plant. Both halves are here because the point is that they now agree.
  WRITERS = {
    'Builder cdata' => ->(c) { Ox::Builder.new(indent: -1) { |b| b.element('r') { b.cdata("a#{c}b") } } },
    'Builder comment' => ->(c) { Ox::Builder.new(indent: -1) { |b| b.comment("a#{c}b") } },
    'Builder doctype' => ->(c) { Ox::Builder.new(indent: -1) { |b| b.doctype("a#{c}b") } },
    'Builder instruct target' => ->(c) { Ox::Builder.new(indent: -1) { |b| b.instruct("a#{c}b") } },
    'Builder instruct version' => ->(c) { Ox::Builder.new(indent: -1) { |b| b.instruct('xml', version: "1#{c}0") } },
    'Builder instruct encoding' => ->(c) { Ox::Builder.new(indent: -1) { |b| b.instruct('xml', encoding: "U#{c}F") } },
    'Builder instruct standalone' => lambda { |c|
      Ox::Builder.new(indent: -1) { |b| b.instruct('xml', standalone: "y#{c}s") }
    },
    'dump CData' => ->(c) { Ox.dump(Ox::Element.new('r') << Ox::CData.new("a#{c}b")) },
    'dump Comment' => ->(c) { Ox.dump(Ox::Element.new('r') << Ox::Comment.new("a#{c}b")) },
    'dump DocType' => ->(c) { Ox.dump(document(Ox::DocType.new("a#{c}b"))) },
    'dump Instruct target' => ->(c) { Ox.dump(document(Ox::Instruct.new("a#{c}b"))) },
    'dump Instruct content' => lambda { |c|
      i = Ox::Instruct.new('xml')
      i.content = "a#{c}b"
      Ox.dump(document(i))
    }
  }.freeze

  # A DocType or an Instruct is only written as part of a Document.
  def self.document(node)
    d = Ox::Document.new
    d << node
    d << Ox::Element.new('r')
    d
  end

  def written(writer, byte)
    writer.call(byte)
  rescue Ox::SyntaxError => e
    e.message
  end

  def test_every_writer_refuses_a_character_xml_can_not_hold
    ["\x00", "\x01", "\x1f"].each do |c|
      WRITERS.each do |where, writer|
        assert_raise(Ox::SyntaxError, "#{where} #{c.inspect}") { writer.call(c) }
      end
    end
  end

  # The whole of the C0 range, so the three XML does allow are not swept up with
  # the rest.
  def test_the_c0_range_and_its_three_exceptions
    (0..0x1f).each do |n|
      c = n.chr
      allowed = ["\t", "\n", "\r"].include?(c)
      WRITERS.each do |where, writer|
        label = format('%s 0x%02x', where, n)
        if allowed
          assert_nothing_raised(label) { writer.call(c) }
        else
          assert_raise(Ox::SyntaxError, label) { writer.call(c) }
        end
      end
    end
  end

  def test_the_message_names_the_byte
    assert_match(/#x01/, written(WRITERS['dump Comment'], "\x01"))
    assert_match(/#x00/, written(WRITERS['Builder cdata'], "\x00"))
  end

  # This is the one that lost data rather than merely writing something invalid.
  def test_a_nul_no_longer_truncates_the_document
    assert_raise(Ox::SyntaxError) { Ox.dump(Ox::Element.new('r') << Ox::Comment.new("a\0b")) }
    assert_raise(Ox::SyntaxError) { Ox.dump(Ox::Element.new('r') << Ox::CData.new("a\0b")) }
    assert_raise(Ox::SyntaxError) { Ox.dump(self.class.document(Ox::DocType.new("a\0b"))) }
    assert_raise(Ox::SyntaxError) { Ox.dump(self.class.document(Ox::Instruct.new("a\0b"))) }
  end

  # Ox::Raw says it adds what it is given without modification, so it is the one
  # value node that is left alone.
  def test_raw_is_still_untouched
    assert_equal("<r>a\x01b</r>".b, Ox::Builder.new(indent: -1) { |b| b.element('r') { b.raw("a\x01b") } })
    assert_equal("\n<r>\n  a\x01b\n</r>\n".b, Ox.dump(Ox::Element.new('r') << Ox::Raw.new("a\x01b")))
  end

  def test_what_xml_does_allow_still_goes_through
    assert_equal("<r><![CDATA[a\tb\nc\rd]]></r>".b,
                 Ox::Builder.new(indent: -1) { |b| b.element('r') { b.cdata("a\tb\nc\rd") } })
    assert_equal('<r><![CDATA[日本語 ☃]]></r>'.b,
                 Ox::Builder.new(indent: -1) { |b| b.element('r') { b.cdata('日本語 ☃') } })
    assert_equal("\n<r>\n  <!--日本語\tx-->\n</r>\n".b,
                 Ox.dump(Ox::Element.new('r') << Ox::Comment.new("日本語\tx")))
    assert_equal('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'.b,
                 Ox::Builder.new(indent: -1) do |b|
                   b.instruct('xml', version: '1.0', encoding: 'UTF-8', standalone: 'yes')
                 end)
  end

  # CDATA passes markup through unescaped, which is the whole point of it, and
  # that has not changed - only the characters XML has no way to write at all.
  def test_cdata_still_passes_markup_through
    assert_equal('<r><![CDATA[a<b&c>d]]></r>'.b,
                 Ox::Builder.new(indent: -1) { |b| b.element('r') { b.cdata('a<b&c>d') } })
    assert_equal('<r><![CDATA[c]]]]><![CDATA[>d]]></r>'.b,
                 Ox::Builder.new(indent: -1) { |b| b.element('r') { b.cdata('c]]>d') } })
    assert_equal("\n<r>\n  <![CDATA[c]]]]><![CDATA[>d]]>\n</r>\n".b,
                 Ox.dump(Ox::Element.new('r') << Ox::CData.new('c]]>d')))
    # The split makes two sections that read back as the three characters, which
    # is what #450 settled on.
    xml = Ox::Builder.new(indent: -1) { |b| b.element('r') { b.cdata('c]]>d') } }
    assert_equal('c]]>d', Ox.parse(xml).nodes.map(&:value).join)
  end

  # The check is made before anything is written, so a caller that rescues and
  # carries on is left with the document it had.
  def test_a_rescued_raise_leaves_nothing_behind
    b = Ox::Builder.new(indent: -1)
    b.element('r')
    b.text('ok')
    assert_raise(Ox::SyntaxError) { b.cdata("a\x01b") }
    assert_raise(Ox::SyntaxError) { b.comment("a\x01b") }
    assert_raise(Ox::SyntaxError) { b.instruct('xml', version: "1\x010") }
    b.pop
    assert_equal('<r>ok</r>'.b, b.to_s)
    assert_equal('ok', Ox.parse(b.to_s).text)
  end

  # An argument that is not a String is still refused by the type check rather
  # than by this one.
  def test_a_non_string_instruct_value_is_unchanged
    assert_raise(Ox::ParseError) { Ox::Builder.new(indent: -1) { |b| b.instruct('xml', version: 1.0) } }
  end
end
