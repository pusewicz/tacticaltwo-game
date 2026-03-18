# frozen_string_literal: true

namespace :cmake do
  desc "Configure CMake RelWithDebInfo build with Ninja"
  task :configure do
    if File.exist?("build/relwithdebinfo/build.ninja")
      puts "Build already configured (build/relwithdebinfo/build.ninja exists)"
    else
      sh "cmake -B build/relwithdebinfo -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_HOT_RELOADING=ON"
    end
  end

  desc "Build RelWithDebInfo configuration"
  task build: :configure do
    sh "cmake --build build/relwithdebinfo"
  end

  namespace :sanitize do
    desc "Configure CMake Sanitize build (ASan + UBSan)"
    task :configure do
      if File.exist?("build/sanitize/build.ninja")
        puts "Sanitize build already configured (build/sanitize/build.ninja exists)"
      else
        sh "cmake -B build/sanitize -G Ninja -DCMAKE_BUILD_TYPE=Sanitize -DENABLE_HOT_RELOADING=ON"
      end
      # Retarget compile_commands.json symlink to sanitize build
      symlink_target = "build/sanitize/compile_commands.json"
      if File.symlink?("compile_commands.json")
        current = File.readlink("compile_commands.json")
        if current != symlink_target
          File.delete("compile_commands.json")
          File.symlink(symlink_target, "compile_commands.json")
          puts "Retargeted compile_commands.json -> #{symlink_target}"
        end
      elsif !File.exist?("compile_commands.json")
        File.symlink(symlink_target, "compile_commands.json")
        puts "Created compile_commands.json -> #{symlink_target}"
      end
    end

    desc "Build Sanitize configuration"
    task build: :configure do
      sh "cmake --build build/sanitize"
    end
  end
end

desc "Build Sanitize configuration (default)"
task default: "cmake:sanitize:build"

desc "Build Sanitize configuration (default)"
task build: "cmake:sanitize:build"

desc "Run the game (Sanitize build)"
task :run do
  sh "ninja -C build/sanitize"
  exec({ "ASAN_OPTIONS" => "halt_on_error=1" }, "build/sanitize/bin/TacticalTwo")
end

desc "Format C source files"
task :format do
  files = FileList["src/**/*.c", "src/**/*.h"]
  sh "clang-format", "-i", *files unless files.empty?
end

def notify(message, title: "TacticalTwo", sound: nil)
  return unless RUBY_PLATFORM.include?("darwin")

  script = %(display notification "#{message}" with title "#{title}")
  script += %( sound name "#{sound}") if sound
  system("osascript", "-e", script)
end

desc "Watch src/ for changes and rebuild game library (Sanitize build)"
task watch: "cmake:sanitize:configure" do
  require "listen"

  listener = Listen.to("src", only: /\.(c|h)$/) do |modified, added, removed|
    puts "\nChanges detected, rebuilding game..."
    if system("ninja -C build/sanitize game")
      notify("Build succeeded")
    else
      notify("Build failed!", sound: "Basso")
    end
  end

  puts "Watching src/ for changes... (Ctrl+C to stop)"
  listener.start
  sleep
end

namespace :analyze do
  desc "Run clang-tidy on all src/ files (advisory report)"
  task clang_tidy: "cmake:sanitize:configure" do
    mkdir_p "reports"
    sdk = `xcrun --show-sdk-path`.strip
    sh "/opt/homebrew/opt/llvm/bin/run-clang-tidy " \
       "-p build/sanitize " \
       "-source-filter '.*/tacticaltwo-game/src/.*' " \
       "-extra-arg=-isysroot#{sdk} " \
       "-quiet 2>&1 | tee reports/clang-tidy.txt; true"
    puts "\nclang-tidy report written to reports/clang-tidy.txt"
  end

  desc "Run cppcheck on src/ (advisory report)"
  task :cppcheck do
    mkdir_p "reports"
    sh "cppcheck " \
       "--project=compile_commands.json " \
       "--std=c23 " \
       "--enable=all " \
       "--suppress=missingIncludeSystem " \
       "--file-filter='*/src/*' " \
       "--output-file=reports/cppcheck.txt " \
       "--error-exitcode=0 2>&1 | tee -a reports/cppcheck.txt; true"
    puts "\ncppcheck report written to reports/cppcheck.txt"
  end

  desc "Run all static analysis (clang-tidy + cppcheck)"
  task all: [:clang_tidy, :cppcheck]
end
