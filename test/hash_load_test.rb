#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: hash mode's finish() callback freeing pi->marked without
# clearing it.
#
# parse.c calls pcb->finish once for every top level entity it yields and once
# more after the loop. The callback freed the mark list but left the pointer,
# mark_cnt and mark_size behind, so:
#
#   * the second call freed the same block again -- "double free or corruption
#     (!prev)" on glibc, and the caller's block runs between the two frees, so
#     the chunk can be reallocated and the second free then releases a live
#     Ruby owned buffer.
#   * a second top level entity had mark_value() write into the freed block, at
#     the stale mark_cnt index.
#
# The other half is the opposite mistake: nothing freed the list at all when the
# parse left early. ox_parse_ensure() handles the helper stack and the circular
# reference table that way and now handles this too. That leak is only visible
# under Valgrind, so test:valgrind is what gates it.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'tmpdir'
require 'ox'

class HashLoadTest < ::Test::Unit::TestCase
  def setup
    # The memcheck lane runs every file in one process. Let the input or XML
    # declaration choose the encoding instead of inheriting a default.
    @opts = Ox.default_options
    Ox.default_options = {encoding: nil, symbolize_keys: true}
  end

  def teardown
    Ox.default_options = @opts
  end

  # finish() runs before the yield and again after the loop.
  def test_block_form_frees_the_mark_list_once
    got = []
    Ox.load('<top a="1"/>', mode: :hash) { |h| got << h }
    assert_equal([{top: [{a: '1'}]}], got)
  end

  # The block reallocates between the two frees, so the second one released a
  # buffer Ruby was using.
  def test_block_that_allocates_between_the_frees
    got = 0
    Ox.load('<top a="1"/>', mode: :hash) do |_h|
      200.times { ' ' * 64 }
      got += 1
    end
    assert_equal(1, got)
  end

  # mark_value() wrote into the freed block for the second entity.
  def test_two_top_level_entities
    got = []
    Ox.load('<a x="1"/><b y="2"/>', mode: :hash) { |h| got << h }
    assert_equal(2, got.size)
    assert_equal({a: [{x: '1'}], b: [{y: '2'}]}, got.last)
  end

  def test_many_top_level_entities
    xml = (1..20).map { |i| %(<e#{i} k="#{i}"/>) }.join
    got = []
    Ox.load(xml, mode: :hash) { |h| got << h }
    assert_equal(20, got.size)
    assert_equal('20', got.last[:e20][0][:k])
  end

  def test_cdata_callbacks_take_the_same_path
    got = []
    Ox.load('<top a="1"><![CDATA[x]]></top>', mode: :hash, with_cdata: true) { |h| got << h }
    assert_equal(1, got.size)
  end

  # The mark list is only built for elements that have attributes, so this is
  # the shape that allocates it and then leaves through the error path.
  # ox_parse_ensure() has to free it; Valgrind is what sees that.
  def test_error_after_the_mark_list_is_allocated
    20.times do
      assert_raise(Ox::ParseError) { Ox.load('<top a="1"><b', mode: :hash) }
    end
  end

  def test_error_in_the_block_leaves_nothing_behind
    20.times do
      assert_raise(RuntimeError) do
        Ox.load('<top a="1"/>', mode: :hash) { |_h| raise 'from the block' }
      end
    end
  end

  # Everything below must keep working.

  def test_without_a_block
    assert_equal({top: [{a: '1'}]}, Ox.load('<top a="1"/>', mode: :hash))
  end

  def test_nested_elements
    assert_equal({top: [{a: '1'}, {c: 't'}]},
                 Ox.load('<top a="1"><c>t</c></top>', mode: :hash))
  end

  def test_hash_no_attrs_mode
    assert_equal({top: 't'}, Ox.load('<top a="1"><top>t</top></top>', mode: :hash_no_attrs)[:top])
  end

  def test_hash_no_attrs_name_encodings
    %w[UTF-8 Shift_JIS ASCII-8BIT].each do |encoding|
      name = '項目'.encode(encoding == 'ASCII-8BIT' ? 'UTF-8' : encoding).force_encoding(encoding)
      xml = '<'.b + name.b + '>one</'.b + name.b + '><'.b + name.b + '>two</'.b + name.b + '>'
      xml.force_encoding(encoding)

      [true, false].each do |symbolize|
        key = symbolize ? name.to_sym : name
        result = Ox.load(xml, mode: :hash_no_attrs, symbolize_keys: symbolize)
        assert_equal({key => %w[one two]}, result)
        assert_equal(name.encoding, result.keys.first.encoding)
      end
    end
  end

  def test_hash_no_attrs_declared_name_encoding
    xml = '<?xml version="1.0" encoding="UTF-8"?><項目/>'.b
    result = Ox.load(xml, mode: :hash_no_attrs, symbolize_keys: false)
    assert_equal({'項目' => nil}, result)
    assert_equal(Encoding::UTF_8, result.keys.first.encoding)
  end

  def test_hash_no_attrs_file_name_defaults_to_binary
    # Unlike load(String), load_file has no input encoding. Without a default
    # or XML declaration, the interned name must retain the binary fallback.
    Dir.mktmpdir('ox-hash-no-attrs') do |dir|
      path = File.join(dir, 'names.xml')
      File.binwrite(path, '<root><項目>1</項目></root>')

      [true, false].each do |symbolize|
        name = '項目'.b
        root_key = symbolize ? :root : 'root'
        name_key = symbolize ? name.to_sym : name
        result = Ox.load_file(path, mode: :hash_no_attrs, symbolize_keys: symbolize)
        assert_equal({root_key => {name_key => '1'}}, result)
        assert_equal(Encoding::ASCII_8BIT, result[root_key].keys.first.encoding)
      end
    end
  end

  def test_hash_no_attrs_name_modifier_receives_encoded_string
    names = []
    modifier = lambda do |name|
      names << name
      name.upcase
    end
    result = Ox.load('<root><項目 ignored="yes">one</項目><項目>two</項目></root>',
                     mode: :hash_no_attrs,
                     element_key_mod: modifier, symbolize_keys: true)
    assert_equal({'ROOT' => {'項目' => %w[one two]}}, result)
    assert_equal(['項目', '項目', 'root'], names)
    assert_equal([Encoding::UTF_8, Encoding::UTF_8, Encoding::US_ASCII], names.map(&:encoding))
  end

  def test_string_keys
    assert_equal({'top' => [{'a' => '1'}]},
                 Ox.load('<top a="1"/>', mode: :hash, symbolize_keys: false))
  end
end
