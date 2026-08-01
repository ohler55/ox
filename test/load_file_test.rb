#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: Ox.load_file sizing the read from an unchecked ftello().
#
# fseek() fails on a stream that can not seek -- a FIFO, a socket, /dev/stdin
# when stdin is a pipe -- and ftello() then returns -1. load_file() did not
# check either, so ALLOCA_N(char, len + 1) allocated nothing and fread() was
# asked for (size_t)-1 bytes into it.
#
# On glibc 2.44 that fread returns 0 with EFAULT and writes nothing, so what
# came out was "Failed to read -1 bytes". Nothing here depends on that: the
# point is that the size is now checked before it is used.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'fileutils'
require 'tmpdir'
require 'test/unit'
require 'ox'

class LoadFileTest < ::Test::Unit::TestCase
  def setup
    @dir = Dir.mktmpdir('ox-load-file')
    # The memcheck lane runs every file in one process, so do not inherit
    # whatever mode ran before this.
    @opts = Ox.default_options
    Ox.default_options = {mode: nil}
  end

  def teardown
    Ox.default_options = @opts
    FileUtils.remove_entry(@dir)
  end

  def path(name)
    File.join(@dir, name)
  end

  def test_regular_file
    p = path('doc.xml')
    File.write(p, '<top><a>x</a></top>')
    assert_equal('x', Ox.load_file(p).nodes[0].nodes[0])
  end

  # Bigger than SMALL_XML so it takes the heap path rather than the stack one.
  def test_file_larger_than_the_stack_threshold
    p = path('big.xml')
    File.write(p, "<top>#{'x' * 10_000}</top>")
    assert_equal(10_000, Ox.load_file(p).nodes[0].size)
  end

  def test_empty_file
    p = path('empty.xml')
    File.write(p, '')
    assert_nothing_raised { Ox.load_file(p) }
  end

  def test_missing_file_raises_io_error
    assert_raise(IOError) { Ox.load_file(path('nope.xml')) }
  end

  # The case the check is for. Opening the FIFO read-write keeps both ends open
  # so neither this nor load_file blocks waiting for the other; fopen inside
  # load_file does not release the GVL, so a writer thread would deadlock.
  def test_unseekable_stream_raises_instead_of_sizing_from_minus_one
    omit('no mkfifo on this platform') unless File.respond_to?(:mkfifo)

    p = path('fifo')
    File.mkfifo(p)
    holder = File.open(p, File::RDWR | File::NONBLOCK)
    begin
      holder.write("<top>#{'A' * 3000}</top>")
      # Was LoadError "Failed to read -1 bytes" before the size was checked.
      assert_raise(IOError) { Ox.load_file(p) }
    ensure
      holder.close
    end
  end

  # fopen succeeds on a directory, and what it then reports as the size is up to
  # the platform: zero on tmpfs, 64 on macOS, and off_t's maximum on the CI's
  # Ubuntu, where allocating it hung Ruby 2.7 and 3.0 outright. Asking whether
  # it is a regular file takes all of that out of the picture.
  def test_directory_raises
    assert_raise(IOError) { Ox.load_file(@dir) }
  end
end
