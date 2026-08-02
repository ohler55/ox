#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: ox opened its files in text mode, so on Windows it could not
# read back what it had written.
#
# All three of ox's own fopen() calls used "r" / "w":
#
#   ox.c       load_file()             fopen(path, "r")
#   dump.c     ox_write_obj_to_file()  fopen(path, "w")
#   builder.c  builder_file()          fopen(..., "w")
#
# On Windows that is text mode. The writers turned every \n into \r\n on the
# way out, and load_file() -- which sizes the read from fstat, the size on
# disk, but reads through the text-mode translation -- came back one byte short
# per line ending and raised
#
#   LoadError: Failed to read N bytes from ...
#
# so any document with a newline in it, which is every document at the default
# indent, could not be loaded back. POSIX makes "rb" and "wb" identical to "r"
# and "w", so **these tests only bite on the Windows CI cells**; on Linux and
# macOS they are guards that pass either way.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'tmpdir'

require 'ox'

class FileBinaryModeTest < ::Test::Unit::TestCase
  DOC = Ox::Element.new('top').tap do |top|
    top[:a] = '1'
    inner = Ox::Element.new('mid')
    inner << 'hello'
    top << inner
    top << Ox::Comment.new('note')
  end

  def in_tmp
    Dir.mktmpdir { |dir| yield File.join(dir, 'o.xml') }
  end

  # The round trip that could not work on Windows.
  def test_to_file_then_load_file
    [-1, 0, 2, 4].each do |indent|
      in_tmp do |path|
        Ox.to_file(path, DOC, indent: indent)
        assert_nothing_raised("indent #{indent}") { Ox.load_file(path) }
        assert_equal(Ox.dump(DOC, indent: indent), Ox.dump(Ox.load_file(path), indent: indent), "indent #{indent}")
      end
    end
  end

  def test_builder_file_then_load_file
    [-1, 0, 2, 4].each do |indent|
      in_tmp do |path|
        Ox::Builder.file(path, indent: indent) do |b|
          b.element('top', 'a' => '1') { b.element('mid') { b.text('hello') } }
        end
        assert_nothing_raised("indent #{indent}") { Ox.load_file(path) }
      end
    end
  end

  # What the writers put on disk must be what the string form says, with no
  # line endings rewritten under them.
  def test_to_file_bytes_match_dump
    [-1, 0, 2, 4].each do |indent|
      in_tmp do |path|
        Ox.to_file(path, DOC, indent: indent)
        assert_equal(Ox.dump(DOC, indent: indent), File.binread(path), "indent #{indent}")
      end
    end
  end

  def test_builder_file_bytes_match_builder_string
    blk = proc { |b| b.element('top') { b.element('mid') { b.text('hello') } } }
    [-1, 0, 2, 4].each do |indent|
      in_tmp do |path|
        Ox::Builder.file(path, indent: indent, &blk)
        assert_equal(Ox::Builder.new(indent: indent, &blk), File.binread(path), "indent #{indent}")
      end
    end
  end

  # A file that already holds CRLF, whoever wrote it, must load. The parser
  # folds \r\n to \n itself (fix_newlines, parse.c), so the result is the same
  # document either way.
  def test_a_crlf_file_loads_and_matches_the_lf_one
    lf = "<?xml version=\"1.0\"?>\n<!-- c1\nc2 -->\n<n a=\"v\">\n  text\n  <m>y</m>\n</n>\n"
    in_tmp do |path|
      File.binwrite(path, lf)
      from_lf = Ox.dump(Ox.load_file(path))
      File.binwrite(path, lf.gsub("\n", "\r\n"))
      assert_nothing_raised { Ox.load_file(path) }
      assert_equal(from_lf, Ox.dump(Ox.load_file(path)))
    end
  end

  # A lone \r is folded too, so a classic Mac file is not a special case.
  def test_a_cr_only_file_loads
    in_tmp do |path|
      File.binwrite(path, "<n>\r  <m>y</m>\r</n>\r")
      assert_equal('n', Ox.load_file(path).name)
    end
  end
end
