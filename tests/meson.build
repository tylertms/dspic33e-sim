test_include = include_directories('.', 'support', '../src', '../src/image')
test_definitions = [
  {
    'name': 'event_scheduler',
    'sources': ['architecture/dspic33/timing/events_test.c'],
    'suite': ['core', 'integration'],
  },
  {'name': 'core_api', 'sources': ['architecture/dspic33/api_test.c'], 'suite': ['core', 'unit', 'integration']},
  {
    'name': 'processor_fault',
    'sources': [
      'architecture/dspic33/exceptions/address.c',
      'architecture/dspic33/exceptions/exceptions.c',
      'architecture/dspic33/exceptions/traps.c',
      'architecture/dspic33/exceptions/main.c',
    ],
    'suite': ['core', 'integration'],
  },
  {
    'name': 'processor_timing',
    'sources': [
      'architecture/dspic33/timing/instructions/cycles.c',
      'architecture/dspic33/timing/instructions/errata.c',
      'architecture/dspic33/timing/instructions/main.c',
    ],
    'suite': ['core', 'integration'],
  },
  {
    'name': 'processor_data',
    'sources': [
      'architecture/dspic33/execution/data/loops.c',
      'architecture/dspic33/execution/data/arithmetic.c',
      'architecture/dspic33/execution/data/memory.c',
      'architecture/dspic33/execution/data/dsp.c',
      'architecture/dspic33/execution/data/edge.c',
      'architecture/dspic33/execution/data/main.c',
    ],
    'suite': ['core', 'integration'],
  },
  {
    'name': 'processor_control',
    'sources': [
      'architecture/dspic33/execution/control/branches.c',
      'architecture/dspic33/execution/control/flow.c',
      'architecture/dspic33/execution/control/system.c',
      'architecture/dspic33/execution/control/main.c',
    ],
    'suite': ['core', 'integration'],
  },
  {
    'name': 'processor_encoding_census',
    'sources': ['architecture/dspic33/execution/encoding_census_test.c'],
    'suite': ['core', 'unit', 'integration', 'census'],
    'timeout': 300,
  },
  {
    'name': 'processor_state_census',
    'sources': ['architecture/dspic33/execution/state_census_test.c'],
    'suite': ['core', 'unit', 'integration', 'census'],
  },
  {
    'name': 'uninitialized_data',
    'sources': ['architecture/dspic33/execution/uninitialized_data_test.c'],
    'suite': ['core', 'unit', 'integration'],
  },
  {
    'name': 'peripheral_state_census',
    'sources': ['device/dspic33ep_mu/state_census_test.c'],
    'suite': ['device', 'unit', 'integration', 'census'],
  },
  {'name': 'processor_boundary', 'sources': ['architecture/dspic33/execution/boundary.c'], 'suite': ['core', 'unit']},
  {
    'name': 'processor_addressing_boundary',
    'sources': ['architecture/dspic33/execution/data/addressing_boundary_test.c'],
    'suite': ['core', 'unit'],
  },
  {'name': 'adc', 'sources': ['device/dspic33ep_mu/analog/adc_test.c'], 'suite': ['device', 'integration']},
  {
    'name': 'auxiliary_clock',
    'sources': ['device/dspic33ep_mu/timing/auxiliary_clock_test.c'],
    'suite': ['device', 'integration'],
  },
  {
    'name': 'can',
    'sources': [
      'device/dspic33ep_mu/communication/can/registers.c',
      'device/dspic33ep_mu/communication/can/receive.c',
      'device/dspic33ep_mu/communication/can/bus.c',
      'device/dspic33ep_mu/communication/can/errors.c',
      'device/dspic33ep_mu/communication/can/boundary.c',
      'device/dspic33ep_mu/communication/can/main.c',
    ],
    'suite': ['device', 'integration'],
    'allocation_failure': true,
  },
  {
    'name': 'comparator',
    'sources': ['device/dspic33ep_mu/analog/comparator/registers.c', 'device/dspic33ep_mu/analog/comparator/behavior.c'],
    'suite': ['device', 'integration'],
    'allocation_failure': true,
  },
  {'name': 'core_sfr', 'sources': ['device/dspic33ep_mu/platform/core_sfr_test.c'], 'suite': ['device', 'integration']},
  {'name': 'common', 'sources': ['device/dspic33ep_mu/common/boundary.c'], 'suite': ['device', 'unit']},
  {'name': 'crc', 'sources': ['device/dspic33ep_mu/memory/crc_test.c'], 'suite': ['device', 'integration']},
  {
    'name': 'dci',
    'sources': [
      'device/dspic33ep_mu/communication/dci/registers.c',
      'device/dspic33ep_mu/communication/dci/transfer.c',
      'device/dspic33ep_mu/communication/dci/clock.c',
      'device/dspic33ep_mu/communication/dci/boundary.c',
      'device/dspic33ep_mu/communication/dci/state_census.c',
      'device/dspic33ep_mu/communication/dci/state_matrix.c',
      'device/dspic33ep_mu/communication/dci/main.c',
    ],
    'suite': ['device', 'integration'],
    'allocation_failure': true,
  },
  {'name': 'device_data', 'sources': ['device/dspic33ep_mu/memory/device_data_test.c'], 'suite': ['device', 'unit']},
  {
    'name': 'dma',
    'sources': ['device/dspic33ep_mu/memory/dma_test.c', 'device/dspic33ep_mu/memory/dma/boundary.c'],
    'suite': ['device', 'integration'],
    'allocation_failure': true,
  },
  {'name': 'gpio', 'sources': ['device/dspic33ep_mu/platform/gpio_test.c'], 'suite': ['device', 'integration']},
  {
    'name': 'i2c',
    'sources': [
      'device/dspic33ep_mu/communication/i2c/registers.c',
      'device/dspic33ep_mu/communication/i2c/master.c',
      'device/dspic33ep_mu/communication/i2c/slave.c',
      'device/dspic33ep_mu/communication/i2c/pins.c',
      'device/dspic33ep_mu/communication/i2c/boundary.c',
      'device/dspic33ep_mu/communication/i2c/state_matrix.c',
    ],
    'suite': ['device', 'integration'],
    'allocation_failure': true,
  },
  {
    'name': 'input_capture',
    'sources': [
      'device/dspic33ep_mu/control/input_capture/registers.c',
      'device/dspic33ep_mu/control/input_capture/events.c',
      'device/dspic33ep_mu/control/input_capture/boundary.c',
    ],
    'suite': ['device', 'integration'],
    'allocation_failure': true,
  },
  {
    'name': 'interrupt_control',
    'sources': ['device/dspic33ep_mu/platform/interrupt_control_test.c'],
    'suite': ['device', 'integration'],
  },
  {
    'name': 'peripheral_census',
    'sources': ['device/dspic33ep_mu/platform/peripheral_census_test.c'],
    'suite': ['device', 'unit', 'integration', 'census'],
  },
  {
    'name': 'peripheral_api_boundary',
    'sources': ['device/dspic33ep_mu/platform/peripheral_api_boundary_test.c'],
    'suite': ['device', 'unit'],
    'allocation_failure': true,
  },
  {
    'name': 'nvm',
    'sources': [
      'device/dspic33ep_mu/memory/nvm/registers.c',
      'device/dspic33ep_mu/memory/nvm/commands.c',
      'device/dspic33ep_mu/memory/nvm/flash.c',
      'device/dspic33ep_mu/memory/nvm/main.c',
    ],
    'suite': ['device', 'integration'],
    'dependencies': [dspic33ep_mu_firmware_image_dependency],
  },
  {
    'name': 'oscillator',
    'sources': [
      'device/dspic33ep_mu/timing/oscillator/configuration.c',
      'device/dspic33ep_mu/timing/oscillator/switching.c',
      'device/dspic33ep_mu/timing/oscillator/main.c',
    ],
    'suite': ['device', 'integration'],
  },
  {
    'name': 'output_compare_basic',
    'sources': ['device/dspic33ep_mu/control/output_compare_basic_test.c'],
    'suite': ['device', 'integration'],
  },
  {
    'name': 'output_compare_boundary',
    'sources': ['device/dspic33ep_mu/control/output_compare/boundary_test.c'],
    'suite': ['device', 'unit'],
    'allocation_failure': true,
  },
  {
    'name': 'output_compare_register_matrix',
    'sources': ['device/dspic33ep_mu/control/output_compare/register_matrix_test.c'],
    'suite': ['device', 'unit'],
  },
  {
    'name': 'output_compare_fault',
    'sources': ['device/dspic33ep_mu/control/output_compare_fault_test.c'],
    'suite': ['device', 'integration'],
  },
  {
    'name': 'output_compare_cascade',
    'sources': ['device/dspic33ep_mu/control/output_compare_cascade_test.c'],
    'suite': ['device', 'integration'],
  },
  {
    'name': 'output_compare_power',
    'sources': ['device/dspic33ep_mu/control/output_compare_power_test.c'],
    'suite': ['device', 'integration'],
  },
  {
    'name': 'output_compare_sync',
    'sources': [
      'device/dspic33ep_mu/control/output_compare_sync/source.c',
      'device/dspic33ep_mu/control/output_compare_sync/synchronization.c',
      'device/dspic33ep_mu/control/output_compare_sync/main.c',
    ],
    'suite': ['device', 'integration'],
  },
  {
    'name': 'output_compare_state_census',
    'sources': ['device/dspic33ep_mu/control/output_compare/state_census_test.c'],
    'suite': ['device', 'unit', 'integration', 'census'],
  },
  {
    'name': 'pmp',
    'sources': [
      'device/dspic33ep_mu/communication/pmp/registers.c',
      'device/dspic33ep_mu/communication/pmp/master.c',
      'device/dspic33ep_mu/communication/pmp/slave.c',
      'device/dspic33ep_mu/communication/pmp/boundary.c',
      'device/dspic33ep_mu/communication/pmp/state_matrix.c',
    ],
    'suite': ['device', 'integration'],
    'allocation_failure': true,
  },
  {'name': 'pps', 'sources': ['device/dspic33ep_mu/platform/pps_test.c'], 'suite': ['device', 'integration']},
  {
    'name': 'pwm',
    'sources': ['device/dspic33ep_mu/control/pwm/configuration.c', 'device/dspic33ep_mu/control/pwm/waveforms.c'],
    'suite': ['device', 'integration'],
  },
  {
    'name': 'qei',
    'sources': ['device/dspic33ep_mu/control/qei/registers.c', 'device/dspic33ep_mu/control/qei/events.c'],
    'suite': ['device', 'integration'],
  },
  {'name': 'reset', 'sources': ['device/dspic33ep_mu/platform/reset_test.c'], 'suite': ['device', 'integration']},
  {'name': 'rtcc', 'sources': ['device/dspic33ep_mu/timing/rtcc_test.c'], 'suite': ['device', 'integration']},
  {
    'name': 'sfr_access',
    'sources': ['device/dspic33ep_mu/platform/sfr_access_test.c'],
    'suite': ['device', 'integration'],
  },
  {
    'name': 'sfr_reset',
    'sources': ['device/dspic33ep_mu/platform/sfr_reset_test.c'],
    'suite': ['device', 'integration'],
  },
  {
    'name': 'spi',
    'sources': [
      'device/dspic33ep_mu/communication/spi/registers.c',
      'device/dspic33ep_mu/communication/spi/master.c',
      'device/dspic33ep_mu/communication/spi/slave.c',
      'device/dspic33ep_mu/communication/spi/boundary.c',
      'device/dspic33ep_mu/communication/spi/state_matrix.c',
    ],
    'suite': ['device', 'integration'],
    'allocation_failure': true,
  },
  {'name': 'timer', 'sources': ['device/dspic33ep_mu/timing/timer_test.c'], 'suite': ['device', 'integration']},
  {
    'name': 'uart',
    'sources': [
      'device/dspic33ep_mu/communication/uart_test.c',
      'device/dspic33ep_mu/communication/uart/boundary.c',
      'device/dspic33ep_mu/communication/uart/state_matrix.c',
    ],
    'suite': ['device', 'integration'],
    'allocation_failure': true,
  },
  {
    'name': 'usb',
    'sources': [
      'device/dspic33ep_mu/communication/usb/registers.c',
      'device/dspic33ep_mu/communication/usb/transactions.c',
      'device/dspic33ep_mu/communication/usb/boundary.c',
    ],
    'suite': ['device', 'integration'],
  },
  {'name': 'watchdog', 'sources': ['device/dspic33ep_mu/timing/watchdog_test.c'], 'suite': ['device', 'integration']},
  {
    'name': 'firmware_image',
    'sources': ['system/firmware_image_test.c'],
    'suite': ['runner', 'system'],
    'dependencies': [dspic33ep_mu_firmware_image_dependency],
  },
]

allocation_failure_tests = cc.has_link_argument('-Wl,--wrap=realloc')
test_link_args = []
if host_machine.system() == 'windows'
  test_link_args = cc.get_supported_link_arguments(
    [
      '-Wl,--stack,33554432',
      '/STACK:33554432',
    ],
  )
endif

foreach definition : test_definitions
  if definition.get('allocation_failure', false) and not allocation_failure_tests
    continue
  endif
  name = 'dspic33ep_mu_' + definition['name'] + '_test'
  sources = definition['sources']
  c_args = []
  link_args = test_link_args
  if definition.get('allocation_failure', false)
    sources += ['support/allocation_failure.c']
    c_args += ['-DDSPIC33_TEST_ALLOCATION_FAILURE=1']
    link_args += ['-Wl,--wrap=realloc']
  endif
  target = executable(
    name,
    sources,
    c_args: c_args,
    dependencies: definition.get('dependencies', [dspic33ep_mu_simulator_dependency]),
    include_directories: test_include,
    link_args: link_args,
  )
  test(name, target, suite: definition['suite'], timeout: definition.get('timeout', 30))
  if definition['name'] == 'processor_encoding_census'
    foreach shard : range(1, 16)
      test(
        name + '_shard_' + shard.to_string(),
        target,
        args: shard.to_string(),
        suite: definition['suite'],
        timeout: definition['timeout'],
      )
    endforeach
  endif
endforeach

smoke_image_name = 'dspic33ep_mu_firmware_runner_smoke.bin'
smoke_image = meson.current_build_dir() / smoke_image_name
fixture_generator = executable(
  'dspic33ep_mu_firmware_runner_fixture',
  'system/runner_fixture_test.c',
  c_args: ['-DDSPIC33EP_MU_RUNNER_SMOKE_IMAGE="' + smoke_image + '"'],
  include_directories: [test_include, public_include],
)
runner_image = custom_target(
  'dspic33ep_mu_firmware_runner_image',
  build_by_default: true,
  command: fixture_generator,
  output: smoke_image_name,
)

test(
  'dspic33ep_mu_firmware_runner_smoke_test',
  dspic33ep_mu_firmware_runner,
  args: [
    runner_image,
    '--reset-address',
    '0',
    '--device',
    'dsPIC33EP256MU806',
    '--program-word',
    '0',
    '0x2005a0',
    '--program-word',
    '2',
    '0xfe4000',
    '--stop-address',
    '2',
    '--max-instructions',
    '10',
  ],
  suite: ['runner', 'system'],
)
test(
  'dspic33ep_mu_firmware_runner_trap_test',
  dspic33ep_mu_firmware_runner,
  args: [
    runner_image,
    '--reset-address',
    '0',
    '--device',
    'dsPIC33EP256MU806',
    '--program-word',
    '0',
    '0xd80103',
    '--program-word',
    '12',
    '0x000300',
    '--program-word',
    '768',
    '0x000000',
    '--max-instructions',
    '10',
  ],
  expected_fail: true,
  suite: ['runner', 'system'],
)
