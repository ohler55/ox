require 'test/ox/hasprops'
require 'test/ox/group'
require 'test/ox/layer'
require 'test/ox/line'
require 'test/ox/shape'
require 'test/ox/oval'
require 'test/ox/rect'
require 'test/ox/text'
require 'test/ox/change'

module Test
  module Ox
    class Doc
      include HasProps

      attr_accessor :title
      attr_accessor :create_time
      attr_accessor :user
      # Hash of layers in the document indexed by layer name.
      attr_reader :layers
      attr_reader :change_history

      def initialize(title)
        @title = title
        @user = ENV['USER']
        @create_time = Time.now
        @layers = {}
        @change_history = []
      end

      def add_change(comment, time=nil, user=nil)
        @change_history << Change.new(comment, time, user)
      end
    end # Doc
  end # Ox
end # Test
