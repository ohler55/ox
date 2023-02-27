require 'etc'

module Test
  module Ox
    class Dir < File
      attr_accessor :files

      def initialize(filename)
        super
        @files = []
      end
      
      def <<(f)
        @files << f
      end
    end # Dir
  end # Ox
end # Test
