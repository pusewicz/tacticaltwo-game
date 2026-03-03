MRuby::Build.new do |conf|
  conf.toolchain :clang

  # On macOS, Homebrew GNU ar conflicts with Apple ld. Force Apple's ar.
  if RUBY_PLATFORM.include?('darwin')
    conf.archiver do |archiver|
      archiver.command = '/usr/bin/ar'
      archiver.archive_options = 'rcs %{outfile} %{objs}'
    end
  end

  # Required: game lib is a shared library
  conf.cc.flags << '-fPIC'
  conf.cc.flags << '-O2'

  # Core standard library (includes mruby-fiber for coroutines)
  conf.gembox 'stdlib'

  # Extensions: sprintf, Time, Struct, Data, Random
  conf.gembox 'stdlib-ext'

  # Math: sin/cos, Rational, Complex, Bigint
  conf.gembox 'math'

  # Runtime script loading (eval + compiler)
  conf.gem :core => 'mruby-eval'
  conf.gem :core => 'mruby-compiler'
  conf.gem :core => 'mruby-metaprog'

  # Intentionally excluded:
  # - stdlib-io (no direct filesystem/network access from scripts)
  # - mruby-bin-* (no standalone executables needed)
  # - metaprog gembox (too heavyweight; cherry-pick mruby-metaprog only)
end
