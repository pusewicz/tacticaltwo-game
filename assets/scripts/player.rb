Fiber.new do |entity|
  puts "Player script started (#{entity.inspect})"

  loop do
    x, y = entity.position
    puts "Player position: #{x.round(1)}, #{y.round(1)}"

    Fiber.yield
  end
end
