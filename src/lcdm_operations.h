#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "lcdm.h"

namespace puloon {

	class lcdm::operation {
		public:
			virtual ~operation() = default;

			virtual command get_command() const = 0;
			virtual void handle_result(const std::vector<std::uint8_t>& result_data) = 0;
			virtual bool is_completed() const = 0;
			virtual void set_error() = 0;

		protected:
			operation() = default;
	};

	class lcdm::purge_operation : public operation {
		public:
			purge_operation();
			virtual ~purge_operation() override = default;

			// purge command data structure:
			// no command data
			virtual command get_command() const override;
			virtual void handle_result(const std::vector<std::uint8_t>& result_data) override;
			virtual bool is_completed() const override;
			virtual void set_error() override;
			std::future<operation_status> get_future_result();

		private:
			bool operation_is_completed;
			bool error;
			std::promise<operation_status> result;

			static const std::size_t result_data_size = 2;
	};

	class lcdm::dispense_operation : public operation {
		public:
			dispense_operation(const bill_quantity_by_cassette& requested_bills);
			virtual ~dispense_operation() override = default;

			// dispense command data structure:
			// [upper] tens,
			// [upper] units,
			// [lower tens],
			// [lower units]
			virtual command get_command() const override;
			virtual void handle_result(const std::vector<std::uint8_t>& result_data) override;
			virtual bool is_completed() const override;
			virtual void set_error() override;
			std::future<dispense_result> get_future_result();

		private:
			// reads tens and units
			// from a result data and
			// converts them into bills count
			std::uint32_t read_bills_count(const std::vector<std::uint8_t>& result_data, const std::size_t offset) const;
			// converts bills count
			// into tens and units
			// and writes them to a command data
			void write_bills_count(std::vector<std::uint8_t>& command_data, std::uint32_t bills_count) const;
			dispense_result build_result(operation_status status) const;

		private:
			std::uint32_t upper_bills_to_dispense;
			std::uint32_t lower_bills_to_dispense;
			std::uint32_t upper_dispensed_bills;
			std::uint32_t lower_dispensed_bills;
			std::uint32_t upper_rejected_bills;
			std::uint32_t lower_rejected_bills;
			bool error;
			std::promise<dispense_result> result;

			static const std::uint32_t max_dispensable_bills = 60;
			static const std::size_t single_cassette_result_data_size = 9;
			static const std::size_t double_cassette_result_data_size = 16;
	};

}

#endif // OPERATIONS_H
