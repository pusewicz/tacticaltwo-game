Fiber.new do |_entity|
  count = 0
  loop do
    sleep 1
    puts "Hello, #{count}"
    count += 1
  end
end
