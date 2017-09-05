# Check that given variables are set and all have non-empty values,
# die with an error otherwise.
#
# Params:
#   1. Variable name(s) to test.
#   2. (optional) Error message to print.
check_defined = \
    $(strip $(foreach 1,$1, \
        $(call __check_defined,$1,$(strip $(value 2)))))
__check_defined = \
    $(if $(value $1),, \
      $(error Undefined $1$(if $2, ($2))))

ifeq ($(PRU),0)
	PRUDEV="4a334000.pru0"
else
	PRUDEV="4a338000.pru1"
endif

all:
	$(error Must specify target either "encoder" or "decoder")

check_args:
	@:$(call check_defined, PRU, which PRU firmware should be compiled for: 0 or 1)
	@:$(call check_defined, GPIO, which pin should be used for i/o e.g. P8_45)

build_dir:
	@mkdir -p build

decoder: check_args build_dir
	@cd firmware/ppm_decoder && make
	@cp firmware/ppm_decoder/gen/ppm_decoder.out build/
	@echo "PPM decoder firmware has been built and placed at build/ppm_decoder.out"
	@echo "Install it and restart the PRU by running these commands:"
	@echo "  cp build/ppm_decoder.out /lib/firmware/am335x-pru$(PRU)-fw"
	@echo "  echo \"$(PRUDEV)\" > /sys/bus/platform/drivers/pru-rproc/unbind 2>/dev/null"
	@echo "  echo \"$(PRUDEV)\" > /sys/bus/platform/drivers/pru-rproc/bind"

encoder: check_args build_dir
	@cd firmware/ppm_encoder && make
	@cp firmware/ppm_encoder/gen/ppm_encoder.out build/
	@echo "PPM encoder firmware has been built and placed at build/ppm_encoder.out"
	@echo "Install it and restart the PRU by running these commands:"
	@echo "  cp build/ppm_encoder.out /lib/firmware/am335x-pru$(PRU)-fw"
	@echo "  echo \"$(PRUDEV)\" > /sys/bus/platform/drivers/pru-rproc/unbind 2>/dev/null"
	@echo "  echo \"$(PRUDEV)\" > /sys/bus/platform/drivers/pru-rproc/bind"
