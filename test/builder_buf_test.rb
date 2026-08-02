#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test for three things in the Ox::Builder output buffer.
#
#   * buf_cleanup() released with libc free() what buf_init() and the two grow
#     paths take from ALLOC_N / REALLOC_N. Reached by Ox::Builder.new(size:)
#     over the 16384 byte inline buffer. sax.c had the same mismatch on the
#     element name that ox_strndup() copies for names of 128 bytes or more.
#     Neither is visible from Ruby -- Valgrind and a Ruby built against a
#     different allocator are what see it -- so these only exercise the paths.
#
#   * A file builder wrote a string straight to the fd when it was at least
#     16384 bytes rather than when it did not fit the buffer, which is the
#     wrong test once :size asks for a larger buffer.
#
#   * append_indent() decided "am I at the start of the document" by asking
#     whether the buffer held anything. A file builder empties the buffer on
#     every flush, so the indent after a chunk that got written straight out
#     was dropped. That is what the file-versus-string comparison below pins:
#     80 of its 336 cases differ on the unfixed code.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'tmpdir'
require 'stringio'

require 'ox'

class BuilderBufTest < ::Test::Unit::TestCase
  BASE = 16_384  # struct _buf's inline base

  # Straddles the inline buffer and each :size below.
  LENGTHS = [0, 1, 255, 4095, BASE - 2, BASE - 1, BASE, BASE + 1, 20_000, 32_767, 32_768, 65_536].freeze
  SIZES   = [nil, 1024, BASE, BASE + 1, 32_768, 65_536].freeze
  INDENTS = [-1, 0, 2, 4].freeze

  # builder_file() opens with fopen(path, "w"), which is text mode on Windows,
  # so every \n reaches the disk as \r\n. These tests are about content, not
  # line endings.
  def read_back(path)
    File.binread(path).gsub("\r\n", "\n")
  end

  def document(text)
    proc do |b|
      b.element('n') do
        b.text(text)
        b.cdata('c')
        b.element('m') { b.text('y') }
      end
    end
  end

  # The gate. A file builder and a string builder must produce the same bytes
  # whatever the chunk sizes do to the buffer.
  def test_file_output_matches_string_output
    mismatched = []
    Dir.mktmpdir do |dir|
      path = File.join(dir, 'o.xml')
      SIZES.each do |size|
        LENGTHS.each do |len|
          INDENTS.each do |indent|
            opts = {indent: indent}
            opts[:size] = size if size
            blk = document('x' * len)
            Ox::Builder.file(path, **opts, &blk)
            mismatched << "size=#{size.inspect} len=#{len} indent=#{indent}" if read_back(path) != Ox::Builder.new(**opts, &blk)
          end
        end
      end
    end
    assert_equal([], mismatched)
  end

  # The indent is not dropped after a chunk large enough to be written straight
  # to the fd.
  def test_indent_survives_a_chunk_larger_than_the_buffer
    Dir.mktmpdir do |dir|
      path = File.join(dir, 'o.xml')
      Ox::Builder.file(path, indent: 2) do |b|
        b.element('n') do
          b.text('x' * (BASE * 2))
          b.element('m') { b.text('y') }
        end
      end
      assert_include(read_back(path), "\n  <m>y</m>\n")
    end
  end

  # A :size larger than the inline buffer must actually be used, rather than
  # every chunk over 16384 bytes going straight out. Needs two chunks: one
  # alone never fills a 32768 byte buffer, so nothing flushes and the two
  # tests agree by accident.
  def test_larger_size_buffers_more
    Dir.mktmpdir do |dir|
      path = File.join(dir, 'o.xml')
      blk  = proc do |b|
        b.element('n') do
          b.text('x' * 20_000)
          b.cdata('c')
          b.text('y' * 20_000)
          b.element('m') { b.text('z') }
        end
      end
      Ox::Builder.file(path, indent: 2, size: 32_768, &blk)
      assert_equal(Ox::Builder.new(indent: 2, size: 32_768, &blk), read_back(path))
    end
  end

  # Exercises buf_init()'s ALLOC_N and the two grow paths, then drops the
  # builders so buf_cleanup() runs. Valgrind grades this one.
  def test_oversized_buffers_are_released
    assert_nothing_raised do
      50.times do
        b = Ox::Builder.new(size: BASE * 4)
        b.element('n') { b.text('x' * 100_000) }
        b.to_s
        b = nil
      end
      GC.start
    end
  end

  # sax.c copies an element name of 128 bytes or more with ox_strndup().
  def test_long_sax_element_names_are_released
    handler = Class.new(Ox::Sax) do
      attr_reader :count

      def initialize
        @count = 0
      end

      def start_element(_name)
        @count += 1
      end
    end.new
    xml = (0...200).map { |i| "<#{'e' * 200}#{i}/>" }.join
    Ox.sax_parse(handler, StringIO.new("<top>#{xml}</top>"))
    assert_equal(201, handler.count)
  end
end
