#include "lcdm_operations.h"
#include <cassert>
#include <exception>

using namespace puloon;

const std::map<std::uint8_t, lcdm::operation_status> operation_statuses_by_code {
	{ 0x30, lcdm::operation_status::good },
	{ 0x31, lcdm::operation_status::normal_stop },
	{ 0x32, lcdm::operation_status::pickup_error },
	{ 0x33, lcdm::operation_status::jam },
	{ 0x34, lcdm::operation_status::overflow_bill },
	{ 0x35, lcdm::operation_status::jam },
	{ 0x36, lcdm::operation_status::jam },
	{ 0x37, lcdm::operation_status::device_error },
	{ 0x38, lcdm::operation_status::bill_end },
	{ 0x3a, lcdm::operation_status::counting_error },
	{ 0x3b, lcdm::operation_status::note_request_error },
	{ 0x3c, lcdm::operation_status::counting_error },
	{ 0x3d, lcdm::operation_status::counting_error },
	{ 0x3f, lcdm::operation_status::device_error },
	{ 0x40, lcdm::operation_status::bill_end },
	{ 0x41, lcdm::operation_status::device_error },
	{ 0x42, lcdm::operation_status::jam },
	{ 0x43, lcdm::operation_status::timeout },
	{ 0x44, lcdm::operation_status::over_reject },
	{ 0x45, lcdm::operation_status::device_error },
	{ 0x46, lcdm::operation_status::device_error },
	{ 0x47, lcdm::operation_status::timeout },
	{ 0x48, lcdm::operation_status::jam },
	{ 0x49, lcdm::operation_status::device_error },
	{ 0x4a, lcdm::operation_status::device_error },
	{ 0x4c, lcdm::operation_status::jam },
	{ 0x4e, lcdm::operation_status::jam }
};

lcdm::purge_operation::purge_operation() :
	operation(),
	operation_is_completed(false),
	error(false),
	result() { }

lcdm::command lcdm::purge_operation::get_command() const {
	if (this->is_completed()) {
		throw std::exception("operation is completed");
	}

	return command(command_code::purge, std::vector<std::uint8_t>(), result_data_size);
}

void lcdm::purge_operation::handle_result(const std::vector<std::uint8_t>& result_data) {
	if (this->is_completed()) {
		throw std::exception("operation is completed");
	}

	if (result_data.size() == result_data_size) {
		if ((command_code)result_data[0] == command_code::purge) {
			try {
				this->result.set_value(operation_statuses_by_code.at(result_data[1]));
				this->operation_is_completed = true;
			} catch (std::out_of_range) {
				this->result.set_exception(std::make_exception_ptr(std::exception("unknown operation status")));
				this->operation_is_completed = true;
			}
		} else {
			throw std::exception("unexpected command");
		}
	} else {
		throw std::exception("incorrect result data format");
	}
}

bool lcdm::purge_operation::is_completed() const {
	return ((this->operation_is_completed) || (this->error));
}

void lcdm::purge_operation::set_error() {
	this->result.set_value(operation_status::connection_error);
	this->error = true;
}

std::future<lcdm::operation_status> lcdm::purge_operation::get_future_result() {
	return this->result.get_future();
}

lcdm::dispense_operation::dispense_operation(const bill_quantity_by_cassette& requested_bills) :
	operation(),
	upper_bills_to_dispense(0),
	lower_bills_to_dispense(0),
	upper_dispensed_bills(0),
	lower_dispensed_bills(0),
	upper_rejected_bills(0),
	lower_rejected_bills(0),
	error(false),
	result() {
	bill_quantity_by_cassette::const_iterator cassette_iterator = requested_bills.find(((std::uint32_t)cassette::upper));

	if (cassette_iterator != requested_bills.end()) {
		// upper cassette is requested
		cassette_iterator = requested_bills.find(((std::uint32_t)cassette::lower));

		if (cassette_iterator != requested_bills.end()) {
			// lower cassette is requested
			this->upper_bills_to_dispense = requested_bills.at((std::uint32_t)cassette::upper);
			this->lower_bills_to_dispense = requested_bills.at((std::uint32_t)cassette::lower);
		} else {
			// only upper cassette is requested
			this->upper_bills_to_dispense = requested_bills.at((std::uint32_t)cassette::upper);
		}
	} else {
		cassette_iterator = requested_bills.find(((std::uint32_t)cassette::lower));

		if (cassette_iterator != requested_bills.end()) {
			// only lower cassette is requested
			this->lower_bills_to_dispense = requested_bills.at((std::uint32_t)cassette::lower);
		} else {
			// no cassette is requested
			throw std::exception("invalid dispense request");
		}
	}
}

lcdm::command lcdm::dispense_operation::get_command() const {
	if (this->error) {
		throw std::exception("operation is completed");
	}

	if (this->upper_bills_to_dispense > 0) {
		// upper cassette is requested
		if (this->lower_bills_to_dispense > 0) {
			// lower cassette is requested
			// upper and lower dispense command is generated
			std::vector<std::uint8_t> current_command_data;
			this->write_bills_count(current_command_data, this->upper_bills_to_dispense);
			this->write_bills_count(current_command_data, this->lower_bills_to_dispense);
			return command(command_code::up_low_dispense, current_command_data, double_cassette_result_data_size);
		} else {
			// only upper cassette is requested
			// upper dispense command is generated
			std::vector<std::uint8_t> current_command_data;
			this->write_bills_count(current_command_data, this->upper_bills_to_dispense);
			return command(command_code::upper_dispense, current_command_data, single_cassette_result_data_size);
		}
	} else if (this->lower_bills_to_dispense > 0) {
		// only lower cassette is requested
		// lower dispense command is generated
		std::vector<std::uint8_t> current_command_data;
		this->write_bills_count(current_command_data, this->lower_bills_to_dispense);
		return command(command_code::lower_dispense, current_command_data, single_cassette_result_data_size);
	} else {
		// no bills to dispense
		throw std::exception("operation is completed");
	}
}

void lcdm::dispense_operation::handle_result(const std::vector<std::uint8_t>& result_data) {
	if (this->is_completed()) {
		throw std::exception("operation is completed");
	}

	if (result_data.size() == single_cassette_result_data_size) {
		if (result_data[0] == (std::uint8_t)command_code::upper_dispense) {
			std::uint32_t bills_passed_exit_sensor = this->read_bills_count(result_data, 3);
			this->upper_bills_to_dispense -= bills_passed_exit_sensor;
			this->upper_dispensed_bills += bills_passed_exit_sensor;
			this->upper_rejected_bills += this->read_bills_count(result_data, 7);

			operation_status current_operation_status = operation_statuses_by_code.at(result_data[5]);

			if ((current_operation_status != operation_status::good)
				&& (current_operation_status != operation_status::normal_stop)) {
				this->result.set_value(this->build_result(current_operation_status));
				this->error = true;
			} else if ((this->upper_bills_to_dispense == 0) && (this->lower_bills_to_dispense == 0)) {
				this->result.set_value(this->build_result(current_operation_status));
			}
		} else if (result_data[0] == (std::uint8_t)command_code::lower_dispense) {
			std::uint32_t bills_passed_exit_sensor = this->read_bills_count(result_data, 3);
			this->lower_bills_to_dispense -= bills_passed_exit_sensor;
			this->lower_dispensed_bills += bills_passed_exit_sensor;
			this->lower_rejected_bills += this->read_bills_count(result_data, 7);

			operation_status current_operation_status = operation_statuses_by_code.at(result_data[5]);

			if ((current_operation_status != operation_status::good)
				&& (current_operation_status != operation_status::normal_stop)) {
				this->result.set_value(this->build_result(current_operation_status));
				this->error = true;
			} else if ((this->upper_bills_to_dispense == 0) && (this->lower_bills_to_dispense == 0)) {
				this->result.set_value(this->build_result(current_operation_status));
			}
		} else {
			throw std::exception("unexpected command");
		}
	} else if (result_data.size() == double_cassette_result_data_size) {
		if (result_data[0] == (std::uint8_t)command_code::up_low_dispense) {
			std::uint32_t bills_passed_exit_sensor = this->read_bills_count(result_data, 3);
			this->upper_bills_to_dispense -= bills_passed_exit_sensor;
			this->upper_dispensed_bills += bills_passed_exit_sensor;
			this->upper_rejected_bills += this->read_bills_count(result_data, 12);

			bills_passed_exit_sensor = this->read_bills_count(result_data, 7);
			this->lower_bills_to_dispense -= bills_passed_exit_sensor;
			this->lower_dispensed_bills += bills_passed_exit_sensor;
			this->lower_rejected_bills += this->read_bills_count(result_data, 14);

			operation_status current_operation_status = operation_statuses_by_code.at(result_data[9]);

			if ((current_operation_status != operation_status::good)
				&& (current_operation_status != operation_status::normal_stop)) {
				this->result.set_value(this->build_result(current_operation_status));
				this->error = true;
			} else if ((this->upper_bills_to_dispense == 0) && (this->lower_bills_to_dispense == 0)) {
				this->result.set_value(this->build_result(current_operation_status));
			}
		} else {
			throw std::exception("unexpected command");
		}
	} else {
		throw std::exception("incorrect result data format");
	}
}

bool lcdm::dispense_operation::is_completed() const {
	return (((this->upper_bills_to_dispense == 0) && (this->lower_bills_to_dispense == 0))
		|| this->error);
}

void lcdm::dispense_operation::set_error() {
	this->result.set_value(build_result(operation_status::connection_error));
	this->error = true;
}

std::future<lcdm::dispense_result> lcdm::dispense_operation::get_future_result() {
	return this->result.get_future();
}

std::uint32_t lcdm::dispense_operation::read_bills_count(const std::vector<std::uint8_t>& result_data, const std::size_t offset) const {
	assert(offset + 1 < result_data.size());
	return ((std::uint32_t)(result_data[offset] - '0') * 10)
		+ ((std::uint32_t)(result_data[offset + 1] - '0'));
}

void lcdm::dispense_operation::write_bills_count(std::vector<std::uint8_t>& command_data, std::uint32_t bills_count) const {
	const std::uint32_t normalized_bills_count = std::min(bills_count, max_dispensable_bills);
	const std::uint32_t tens = normalized_bills_count / 10;
	const std::uint32_t units = normalized_bills_count % 10;
	command_data.push_back(tens + '0');
	command_data.push_back(units + '0');
}

lcdm::dispense_result lcdm::dispense_operation::build_result(operation_status status) const {
	dispense_result result;

	result.dispensed_bills[(cassette_number)cassette::upper] = this->upper_dispensed_bills;
	result.dispensed_bills[(cassette_number)cassette::lower] = this->lower_dispensed_bills;
	result.rejected_bills[(cassette_number)cassette::upper] = this->upper_rejected_bills;
	result.rejected_bills[(cassette_number)cassette::lower] = this->lower_rejected_bills;
	result.status = status;

	return result;
}
