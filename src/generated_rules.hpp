#pragma once
#include "tetra_core.hpp"
#include <vector>

namespace tetra::rules {

inline const std::vector<Rule> syncInfoRulesTMO = {

            Rule(GlobalNames::ColorCode, 6 ),
            Rule(GlobalNames::TimeSlot, 2 ),
            Rule(GlobalNames::Frame, 5 ),
            Rule(GlobalNames::MultiFrame, 6 ),
            Rule(GlobalNames::SharingMode, 2 ),
            Rule(GlobalNames::TSReservedFrames, 3 ),
            Rule(GlobalNames::UPlaneDTX, 1 ),
            Rule(GlobalNames::Frame18Extension, 1 ),
            Rule(GlobalNames::Reserved, 1 ),
            Rule(GlobalNames::MCC, 10 ),
            Rule(GlobalNames::MNC, 14 ),
            Rule(GlobalNames::NeighbourCellBroadcast, 2 ),
            Rule(GlobalNames::CellServiceLevel, 2 ),
            Rule(GlobalNames::LateEntryInfo, 1 )
};

inline const std::vector<Rule> dmacSyncInfoRulesDMO = {

            Rule(GlobalNames::Communication_type, 2 ),
            Rule(GlobalNames::Master_slave_link_flag, 1 ),
            Rule(GlobalNames::Gateway_generated_message_flag, 1 ),
            Rule(GlobalNames::A_B_channel_usage, 2 ),
            Rule(GlobalNames::TimeSlot, 2 ),
            Rule(GlobalNames::Frame, 5 ),
            Rule(GlobalNames::Air_interface_encryption, 2 ),
            Rule(GlobalNames::Reserved, 39 )
};

inline const std::vector<Rule> dpresSyncInfoRules = {

            Rule(GlobalNames::Repeater_Communication_type, 2 ),
            Rule(GlobalNames::Repeater_M_DMO_flag, 1 ),
            Rule(GlobalNames::Reserved, 2, RulesType::Switch, (int)GlobalNames::Repeater_Communication_type, 1),
            Rule(GlobalNames::Repeater_Two_frequency_flag, 1, RulesType::Switch, (int)GlobalNames::Repeater_Communication_type, 1),
            Rule(GlobalNames::Repeater_operating_modes, 2, RulesType::Switch, (int)GlobalNames::Repeater_Communication_type, 1),
            Rule(GlobalNames::Repeater_Spacing_of_uplink, 6, RulesType::Switch, (int)GlobalNames::Repeater_Communication_type, 1),
            Rule(GlobalNames::Repeater_Master_slave_link_flag, 1 ),
            Rule(GlobalNames::Repeater_A_B_channel_usage, 2 ),
            Rule(GlobalNames::Repeater_Channel_state, 2 ),
            Rule(GlobalNames::Repeater_TimeSlot, 2 ),
            Rule(GlobalNames::Repeater_Frame, 5 ),
            Rule(GlobalNames::Repeater_Power_class, 3 ),
            Rule(GlobalNames::Repeater_Power_control_flag, 1 ),
            Rule(GlobalNames::Reserved, 1),
            Rule(GlobalNames::Repeater_Frame_countdown, 2),
            Rule(GlobalNames::Repeater_Priority_level, 2),
            Rule(GlobalNames::Reserved, 6, RulesType::Switch, (int)GlobalNames::Repeater_Communication_type, 1),
            Rule(GlobalNames::Values_of_DN232_DN233, 4, RulesType::Switch, (int)GlobalNames::Repeater_Communication_type, 1),
            Rule(GlobalNames::Values_of_DT_254, 3, RulesType::Switch, (int)GlobalNames::Repeater_Communication_type, 1),
            Rule(GlobalNames::Repeater_dual_watch_synchronization_flag, 1),
            Rule(GlobalNames::Reserved, 5, RulesType::Switch, (int)GlobalNames::Repeater_Communication_type, 1),
};

inline const std::vector<Rule> dmacSyncInfoHalfSlotRules = {

            Rule(GlobalNames::Repeater_address, 10 ),
            Rule(GlobalNames::Fill_bit, 1 ),
            Rule(GlobalNames::Fragmentation_flag, 1 ),
            Rule(GlobalNames::Number_of_slots, 4, RulesType::Switch, (int)GlobalNames::Fragmentation_flag, 1 ),
            Rule(GlobalNames::Frame_countdown, 2 ),
            Rule(GlobalNames::Destination_address_type, 2 ),
            Rule(GlobalNames::Destination_address, 24, RulesType::SwitchNot, (int)GlobalNames::Destination_address_type, 2),
            Rule(GlobalNames::Source_address_type, 2 ),
            Rule(GlobalNames::Source_address, 24, RulesType::SwitchNot, (int)GlobalNames::Source_address_type, 2),
            Rule(GlobalNames::MCC, 10),
            Rule(GlobalNames::MNC, 14),
            Rule(GlobalNames::Message_type, 5)
};

inline const std::vector<Rule> dpresSyncInfoHalfSlotRules = {

            Rule(GlobalNames::Repeater_Gateway_address, 10, RulesType::Switch, (int)GlobalNames::Repeater_Communication_type, 1 ),
            Rule(GlobalNames::Repeater_MCC, 10, RulesType::Switch, (int)GlobalNames::Repeater_Communication_type, 1),
            Rule(GlobalNames::Repeater_MNC, 14, RulesType::Switch, (int)GlobalNames::Repeater_Communication_type, 1),
            Rule(GlobalNames::Repeater_Validity_time_unit, 2 ),
            Rule(GlobalNames::Repeater_Number_of_validity_time_units, 6 ),
            Rule(GlobalNames::Repeater_Maximum_DM_MS_power_class, 3),
            Rule(GlobalNames::Reserved, 1),
            Rule(GlobalNames::Repeater_Usage_restriction_type, 4),

            Rule(GlobalNames::Repeater_Usage_SCKN, 5, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 8),
            Rule(GlobalNames::Repeater_Usage_SCKN, 5, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 9),
            Rule(GlobalNames::Repeater_Usage_SCKN, 5, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 10),
             Rule(GlobalNames::Repeater_Usage_SCKN, 5, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 11),

             Rule(GlobalNames::Repeater_Usage_EUIV, 19, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 8),
             Rule(GlobalNames::Repeater_Usage_EUIV, 19, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 9),
             Rule(GlobalNames::Repeater_Usage_EUIV, 19, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 10),
             Rule(GlobalNames::Repeater_Usage_EUIV, 19, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 11),

            Rule(GlobalNames::Repeater_Usage_MCC, 10, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 2),
            Rule(GlobalNames::Repeater_Usage_MNC, 14, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 2),

            Rule(GlobalNames::Repeater_Usage_MCC, 10, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 3),
            Rule(GlobalNames::Repeater_Usage_MNC, 14, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 3),
            Rule(GlobalNames::Repeater_Usage_SSI, 24, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 3),

            Rule(GlobalNames::Repeater_Usage_MCC, 10, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 4),
            Rule(GlobalNames::Repeater_Usage_MNC, 14, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 4),
            Rule(GlobalNames::Repeater_Usage_SSI, 24, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 4),
            Rule(GlobalNames::Repeater_Usage_SSI2, 24, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 4),

            Rule(GlobalNames::Repeater_Usage_MCC, 10, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 5),
            Rule(GlobalNames::Repeater_Usage_MNC, 14, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 5),
            Rule(GlobalNames::Repeater_Usage_SSI, 24, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 5),
            Rule(GlobalNames::Repeater_Usage_SSI2, 24, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 5),

            Rule(GlobalNames::Repeater_Usage_SSI, 24, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 6),
            Rule(GlobalNames::Repeater_Usage_SSI2, 24, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 6),
            Rule(GlobalNames::Repeater_Usage_SSI3, 24, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 6),

            Rule(GlobalNames::Repeater_Usage_MCC, 10, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 8),
            Rule(GlobalNames::Repeater_Usage_MNC, 14, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 8),

            Rule(GlobalNames::Repeater_Usage_MCC, 10, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 9),
            Rule(GlobalNames::Repeater_Usage_MNC, 14, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 9),
            Rule(GlobalNames::Repeater_Usage_SSI, 24, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 9),


            Rule(GlobalNames::Repeater_Usage_SSI, 24, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 10),

            Rule(GlobalNames::Repeater_Usage_SSI, 24, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 11),
            Rule(GlobalNames::Repeater_Usage_SSI2, 24, RulesType::Switch, (int)GlobalNames::Repeater_Usage_restriction_type, 11),
};

inline const std::vector<Rule> sysInfoRules = {

            Rule(GlobalNames::Main_Carrier, 12 ),
            Rule(GlobalNames::Frequency_Band, 4 ),
            Rule(GlobalNames::Offset, 2 ),
            Rule(GlobalNames::Duplex_Spacing, 3 ),
            Rule(GlobalNames::Reverse_Operation, 1 ),
            Rule(GlobalNames::NumberOfCommon_SC, 2 ),
            Rule(GlobalNames::MS_TXPwr_Max_Cell, 3 ),
            Rule(GlobalNames::RXLevel_Access_Min, 4 ),
            Rule(GlobalNames::Access_Parameter, 4 ),
            Rule(GlobalNames::Radio_Downlink_Ttimeout, 4 ),
            Rule(GlobalNames::Hyperframe_or_Cipher_key_flag, 1 ),
            Rule(GlobalNames::Hyperframe, 16, RulesType::Switch, (int)GlobalNames::Hyperframe_or_Cipher_key_flag, 0 ),
            Rule(GlobalNames::Cipher_key, 16, RulesType::Switch, (int)GlobalNames::Hyperframe_or_Cipher_key_flag, 1 ),
            Rule(GlobalNames::Optional_field_flag, 2 ),
            Rule(GlobalNames::Optional_field_value, 20, RulesType::Switch, (int)GlobalNames::Optional_field_flag, 0 ),
            Rule(GlobalNames::Optional_field_value, 20, RulesType::Switch, (int)GlobalNames::Optional_field_flag, 1 ),
            Rule(GlobalNames::Optional_field_value, 20, RulesType::Switch, (int)GlobalNames::Optional_field_flag, 2 ),
            Rule(GlobalNames::Authentication_required_on_cell, 1, RulesType::Switch, (int)GlobalNames::Optional_field_flag, 3 ),
            Rule(GlobalNames::Security_Class_1_supported_on_cell, 1, RulesType::Switch, (int)GlobalNames::Optional_field_flag, 3 ),
            Rule(GlobalNames::Security_Class_3_supported_on_cell, 1, RulesType::Switch, (int)GlobalNames::Optional_field_flag, 3 ),
            Rule(GlobalNames::Optional_field_value, 17, RulesType::Switch, (int)GlobalNames::Optional_field_flag, 3 ),
            Rule(GlobalNames::Location_Area, 14 ),
            Rule(GlobalNames::Subscriber_Class, 16 ),
            Rule(GlobalNames::Registration_required, 1 ),
            Rule(GlobalNames::De_registration_required, 1 ),
            Rule(GlobalNames::Priority_cell, 1 ),
            Rule(GlobalNames::Cell_never_uses_minimum_mode, 1 ),
            Rule(GlobalNames::Migration_supported, 1 ),
            Rule(GlobalNames::System_wide_services, 1 ),
            Rule(GlobalNames::TETRA_voice_service, 1 ),
            Rule(GlobalNames::Circuit_mode_data_service, 1 ),
            Rule(GlobalNames::Reserved, 1),
            Rule(GlobalNames::SNDCP_Service, 1 ),
            Rule(GlobalNames::Air_interface_encryption, 1 ),
            Rule(GlobalNames::Advanced_link_supported, 1 )
};

inline const std::vector<Rule> accessDefineRules = {

            Rule(GlobalNames::Common_or_assigned, 1 ),
            Rule(GlobalNames::Access_code, 2 ),
            Rule(GlobalNames::IMM, 4 ),
            Rule(GlobalNames::WT, 4 ),
            Rule(GlobalNames::Nu, 4 ),
            Rule(GlobalNames::Frame_length_factor, 1 ),
            Rule(GlobalNames::Timeslot_pointer, 4 ),
            Rule(GlobalNames::Minimum_priority, 3 ),
            Rule(GlobalNames::Optional_field_flag, 2 ),
            Rule(GlobalNames::Subscriber_class, 16, RulesType::Switch, (int)GlobalNames::Optional_field_flag, 1),
            Rule(GlobalNames::GSSI, 24, RulesType::Switch, (int)GlobalNames::Optional_field_flag, 2 ),
            Rule(GlobalNames::Filler_bits, 3 )
};

inline const std::vector<Rule> macEndRules = {

            Rule(GlobalNames::Fill_bit, 1 ),
            Rule(GlobalNames::Position_of_grant, 1 ),
            Rule(GlobalNames::Length_indication, 6 ),
            Rule(GlobalNames::Slot_granting_flag, 1, RulesType::Presence_bit, 1 ),
            Rule(GlobalNames::Slot_granting_element, 8 )
};

inline const std::vector<Rule> dmacEndRules = {

            Rule(GlobalNames::Fill_bit, 1 )
};

inline const std::vector<Rule> dmacDataRules = {

            Rule( GlobalNames::Fill_bit, 1 ),
            Rule(GlobalNames::Second_half_slot_stolen, 1 ),
            Rule(GlobalNames::Fragmentation_flag, 1 ),
            Rule(GlobalNames::Null_pdu, 1 ),
            Rule(GlobalNames::Frame_countdown, 2 ),
            Rule(GlobalNames::Air_interface_encryption, 2),
            Rule(GlobalNames::Destination_address_type, 2),
            Rule(GlobalNames::Destination_address, 24, RulesType::SwitchNot, (int) GlobalNames::Destination_address_type, 2),
            Rule(GlobalNames::Source_address_type, 2),
            Rule(GlobalNames::Source_address, 24, RulesType::SwitchNot, (int) GlobalNames::Source_address_type, 2),
            Rule(GlobalNames::MCC, 10),
            Rule(GlobalNames::MNC, 14),
            Rule(GlobalNames::Message_type, 5)
};

inline const std::vector<Rule> macResourceRules = {

            Rule( GlobalNames::Fill_bit, 1 ),
            Rule(GlobalNames::Position_of_grant, 1 ),
            Rule(GlobalNames::Encryption_mode, 2 ),
            Rule(GlobalNames::Random_access_flag, 1 ),
            Rule(GlobalNames::Length_indication, 6 ),
            Rule(GlobalNames::Address_type, 3),
            Rule(GlobalNames::SSI, 24, RulesType::Switch, (int) GlobalNames::Address_type, 1),
            Rule(GlobalNames::Event_Label, 10, RulesType::Switch, (int) GlobalNames::Address_type, 2),
            Rule(GlobalNames::USSI, 24, RulesType::Switch, (int) GlobalNames::Address_type, 3),
            Rule(GlobalNames::SMI, 24, RulesType::Switch, (int) GlobalNames::Address_type, 4),
            Rule(GlobalNames::SSI, 24, RulesType::Switch, (int) GlobalNames::Address_type, 5),
            Rule(GlobalNames::Event_Label, 10, RulesType::Switch, (int) GlobalNames::Address_type, 5),
            Rule(GlobalNames::SSI, 24, RulesType::Switch, (int) GlobalNames::Address_type, 6),
            Rule(GlobalNames::Usage, 6, RulesType::Switch, (int) GlobalNames::Address_type, 6),
            Rule(GlobalNames::SMI, 24, RulesType::Switch, (int) GlobalNames::Address_type, 7),
            Rule(GlobalNames::Event_Label, 10, RulesType::Switch, (int) GlobalNames::Address_type, 7),
            Rule(GlobalNames::Power_control_flag, 1, RulesType::Presence_bit, 1 ),
            Rule(GlobalNames::Power_control_element, 4 ),
            Rule(GlobalNames::Slot_granting_flag, 1, RulesType::Presence_bit, 1 ),
            Rule(GlobalNames::Slot_granting_element, 8 ),
};

inline const std::vector<Rule> channelAllocationRules = {

            Rule(GlobalNames::Channel_allocation_flag, 1, RulesType::Options_bit),
            Rule(GlobalNames::Allocation_type, 2 ),
            Rule(GlobalNames::Timeslot_assigned, 4 ),
            Rule(GlobalNames::Uplink_downlink_assigned, 2 ),
            Rule(GlobalNames::CLCH_permission, 1 ),
            Rule(GlobalNames::Cell_change_flag, 1 ),
            Rule(GlobalNames::Carrier_number, 12 ),
            Rule(GlobalNames::Extended_carrier_numbering_flag, 1, RulesType::Presence_bit, 4),
            Rule(GlobalNames::Extended_frequency_band, 4 ),
            Rule(GlobalNames::Extended_offset, 2 ),
            Rule(GlobalNames::Extended_duplex_spacing, 3 ),
            Rule(GlobalNames::Extended_reverse_operation, 1 ),
            Rule(GlobalNames::Monitoring_pattern, 2 ),
            Rule(GlobalNames::Frame_18_monitoring_pattern, 2, RulesType::Switch, (int)GlobalNames::Monitoring_pattern, 0),
            Rule(GlobalNames::Reserved, 0, RulesType::JampNot, (int)GlobalNames::Uplink_downlink_assigned, 0, 99),
            Rule(GlobalNames::Up_downlink_assigned_for_augmented_ch_alloc, 2),
            Rule(GlobalNames::Bandwidth_of_allocated_channel, 3),
            Rule(GlobalNames::Modulation_mode_of_allocated_channel, 3),
            Rule(GlobalNames::Maximum_uplink_QAM_modulation_level, 3),
            Rule(GlobalNames::Conforming_channel_status, 3),
            Rule(GlobalNames::BS_link_imbalance, 4),
            Rule(GlobalNames::BS_transmit_power_relative_to_main_carrier, 5),
            Rule(GlobalNames::Napping_status, 2),
            Rule(GlobalNames::Napping_information, 11, RulesType::Switch, (int)GlobalNames::Napping_status, 1),
            Rule(GlobalNames::Reserved, 4),
            Rule(GlobalNames::Conditional_element_A_flag, 1, RulesType::Presence_bit, 1),
            Rule(GlobalNames::Conditional_element_A, 16),
            Rule(GlobalNames::Conditional_element_B_flag, 1, RulesType::Presence_bit, 1),
            Rule(GlobalNames::Conditional_element_B, 16),
            Rule(GlobalNames::Further_augmentation_flag, 1)
};

inline const std::vector<Rule> uSignalRules = {

            Rule(GlobalNames::Second_half_slot_stolen, 1 )
};

inline const std::vector<Rule> d_ReleaseRules = {

            Rule(GlobalNames::Call_identifier, 14 ),
            Rule(GlobalNames::Disconnect_cause, 5 ),
            Rule(GlobalNames::Options_bit, 1 , RulesType::Options_bit),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1),
            Rule(GlobalNames::Notification_indicator, 6 ),
            Rule(GlobalNames::More_bit, 1, RulesType::More_bit)
};

inline const std::vector<Rule> d_SdsDataRules = {

            Rule(GlobalNames::Calling_party_type_identifier, 2 ),
            Rule(GlobalNames::Calling_party_address_SSI, 24 ),
            Rule(GlobalNames::Calling_party_extension, 24, RulesType::Switch, (int)GlobalNames::Calling_party_type_identifier, 2),
            Rule(GlobalNames::Short_data_type_identifier, 2 ),
            Rule(GlobalNames::User_Defined_Data_16, 16, RulesType::Switch, (int)GlobalNames::Short_data_type_identifier, 0),
            Rule(GlobalNames::User_Defined_Data_32, 32, RulesType::Switch, (int)GlobalNames::Short_data_type_identifier, 1),
            Rule(GlobalNames::User_Defined_Data_64_1, 32, RulesType::Switch, (int)GlobalNames::Short_data_type_identifier, 2),
            Rule(GlobalNames::User_Defined_Data_64_2, 32, RulesType::Switch, (int)GlobalNames::Short_data_type_identifier, 2),
            Rule(GlobalNames::User_Defined_Data4_Length, 11, RulesType::Switch, (int)GlobalNames::Short_data_type_identifier, 3)
};

inline const std::vector<Rule> d_TX_CeasedRules = {

            Rule(GlobalNames::Call_identifier, 14 ),
            Rule(GlobalNames::Transmission_request_permission, 1 ),
            Rule(GlobalNames::Options_bit, 1 , RulesType::Options_bit),
            Rule(GlobalNames::Presence_bit, 1 , RulesType::Presence_bit, 1),
            Rule(GlobalNames::Notification_indicator, 6 ),
            Rule(GlobalNames::More_bit, 1, RulesType::More_bit)
};

inline const std::vector<Rule> d_TX_GrantedRules = {

            Rule(GlobalNames::Call_identifier, 14),
            Rule(GlobalNames::Transmission_grant, 2),
            Rule(GlobalNames::Transmission_request_permission, 1),
            Rule(GlobalNames::Encryption_control, 1),
            Rule(GlobalNames::Reserved, 1),
            Rule(GlobalNames::Options_bit, 1, RulesType::Options_bit),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1),
            Rule(GlobalNames::Notification_indicator, 6),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 3),
            Rule(GlobalNames::Transmitting_party_type_identifier, 2),
            Rule(GlobalNames::Transmitting_party_address_SSI, 24),
            Rule(GlobalNames::Transmitting_party_extension, 24, RulesType::Switch, (int)GlobalNames::Transmitting_party_type_identifier, 2),
            Rule(GlobalNames::More_bit, 1, RulesType::More_bit)
};

inline const std::vector<Rule> d_setupRules = {

            Rule(GlobalNames::Call_identifier, 14),
            Rule(GlobalNames::Call_time_out, 4 ),
            Rule(GlobalNames::Hook_method, 1 ),
            Rule(GlobalNames::Simplex_duplex, 1 ),
            Rule(GlobalNames::Basic_service_Circuit_mode_type, 3 ),
            Rule(GlobalNames::Basic_service_Encryption_flag, 1 ),
            Rule(GlobalNames::Basic_service_Communication_type, 2 ),
            Rule(GlobalNames::Basic_service_Slots_per_frame, 2, RulesType::SwitchNot, (int)GlobalNames::Basic_service_Circuit_mode_type, 0),
            Rule(GlobalNames::Basic_service_Speech_service, 2, RulesType::Switch, (int)GlobalNames::Basic_service_Circuit_mode_type, 0),
            Rule(GlobalNames::Transmission_grant, 2 ),
            Rule(GlobalNames::Transmission_request_permission, 1 ),
            Rule(GlobalNames::Call_priority, 4 ),
            Rule(GlobalNames::Options_bit, 1, RulesType::Options_bit ),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1 ),
            Rule(GlobalNames::Notification_indicator, 6 ),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1 ),
            Rule(GlobalNames::Temporary_address, 24 ),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 3 ),
            Rule(GlobalNames::Calling_party_type_identifier, 2 ),
            Rule(GlobalNames::Calling_party_address_SSI, 24 ),
            Rule(GlobalNames::Calling_party_extension, 24, RulesType::Switch, (int)GlobalNames::Calling_party_type_identifier, 2),
            Rule(GlobalNames::More_bit, 1 , RulesType::More_bit)
};

inline const std::vector<Rule> d_infoRules = {

            Rule(GlobalNames::Call_identifier, 14),
            Rule(GlobalNames::Reset_Call_time_out, 1 ),
            Rule(GlobalNames::Poll_request, 1 ),
            Rule(GlobalNames::Options_bit, 1, RulesType::Options_bit ),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1 ),
            Rule(GlobalNames::New_Call_Identifier, 14 ),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1 ),
            Rule(GlobalNames::Call_time_out, 4 ),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1 ),
            Rule(GlobalNames::Call_time_out_setup_phase, 3 ),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1 ),
            Rule(GlobalNames::Call_ownership, 1 ),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1 ),
            Rule(GlobalNames::Modify, 9 ),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1 ),
            Rule(GlobalNames::Call_status, 3 ),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1 ),
            Rule(GlobalNames::Temporary_address, 24 ),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1 ),
            Rule(GlobalNames::Notification_indicator, 6 ),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1 ),
            Rule(GlobalNames::Poll_response_percentage, 6 ),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1 ),
            Rule(GlobalNames::Poll_response_number, 6 ),
            Rule(GlobalNames::More_bit, 1 , RulesType::More_bit)
};

inline const std::vector<Rule> d_connectRules = {

            Rule(GlobalNames::Call_identifier, 14),
            Rule(GlobalNames::Call_time_out, 4),
            Rule(GlobalNames::Hook_method, 1),
            Rule(GlobalNames::Simplex_duplex, 1),
            Rule(GlobalNames::Transmission_grant, 2),
            Rule(GlobalNames::Transmission_request_permission, 1),
            Rule(GlobalNames::Call_ownership, 1),
            Rule(GlobalNames::Options_bit, 1, RulesType::Options_bit),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1),
            Rule(GlobalNames::Call_priority, 4),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 5),
            Rule(GlobalNames::Basic_service_Circuit_mode_type, 3 ),
            Rule(GlobalNames::Basic_service_Encryption_flag, 1 ),
            Rule(GlobalNames::Basic_service_Communication_type, 2 ),
            Rule(GlobalNames::Basic_service_Slots_per_frame, 2, RulesType::SwitchNot, (int)GlobalNames::Basic_service_Circuit_mode_type, 0),
            Rule(GlobalNames::Basic_service_Speech_service, 2, RulesType::Switch, (int)GlobalNames::Basic_service_Circuit_mode_type, 0),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1),
            Rule(GlobalNames::Temporary_address, 24),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1),
            Rule(GlobalNames::Notification_indicator, 6),
            Rule(GlobalNames::More_bit, 1, RulesType::More_bit)
};

inline const std::vector<Rule> d_Nwrk_BroadcastRules = {

            Rule(GlobalNames::Cell_reselect_parameters, 16),
            Rule(GlobalNames::Cell_service_level, 2),
            Rule(GlobalNames::Options_bit, 1, RulesType::Options_bit),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 3),
            Rule(GlobalNames::Network_time, 24),
            Rule(GlobalNames::Local_time_offset_sign, 1),
            Rule(GlobalNames::Local_time_offset, 23),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1),
            Rule(GlobalNames::Number_of_Neighbour_cells_element, 3),
};

inline const std::vector<Rule> neighbour_Cell_InfoRules = {

            Rule(GlobalNames::Cell_identifier, 5),
            Rule(GlobalNames::Cell_reselection_types_supported, 2),
            Rule(GlobalNames::Neighbour_cell_synchronised, 1),
            Rule(GlobalNames::Neighbour_cell_service_level, 2),
            Rule(GlobalNames::Main_carrier_number, 12),
            Rule(GlobalNames::Options_bit, 1, RulesType::Options_bit),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1),
            Rule(GlobalNames::Main_carrier_number_extension, 10),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1),
            Rule(GlobalNames::Neighbour_MCC, 10),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1),
            Rule(GlobalNames::Neighbour_MNC, 14),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1),
            Rule(GlobalNames::Neighbour_LA, 14),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1),
            Rule(GlobalNames::Maximum_MS_transmit_power, 3),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1),
            Rule(GlobalNames::Minimum_RX_access_level, 4),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1),
            Rule(GlobalNames::Subscriber_class, 16),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 12),
            Rule(GlobalNames::Registration_required, 1 ),
            Rule(GlobalNames::De_registration_required, 1 ),
            Rule(GlobalNames::Priority_cell, 1 ),
            Rule(GlobalNames::Cell_never_uses_minimum_mode, 1 ),
            Rule(GlobalNames::Migration_supported, 1 ),
            Rule(GlobalNames::System_wide_services, 1 ),
            Rule(GlobalNames::TETRA_voice_service, 1 ),
            Rule(GlobalNames::Circuit_mode_data_service, 1 ),
            Rule(GlobalNames::Reserved, 1 ),
            Rule(GlobalNames::SNDCP_Service, 1 ),
            Rule(GlobalNames::Air_interface_encryption, 1 ),
            Rule(GlobalNames::Advanced_link_supported, 1 ),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1),
            Rule(GlobalNames::Timeshare_cell_and_AI_encryption, 5),
            Rule(GlobalNames::Presence_bit, 1, RulesType::Presence_bit, 1),
            Rule(GlobalNames::TDMA_frame_offset, 6),
};

inline const std::vector<Rule> d_Sds_TL_ForwardRules = {

            Rule(GlobalNames::Delivery_report_request, 2),
            Rule(GlobalNames::Service_selection, 1 ),
            Rule(GlobalNames::Storage_forward_control, 1),
            Rule(GlobalNames::Message_reference, 8),
            Rule(GlobalNames::Reserved, 0, RulesType::Jamp, (int)GlobalNames::Storage_forward_control, 0, 99),
            Rule(GlobalNames::Validity_period, 5),
            Rule(GlobalNames::Forward_address_type, 3),
            Rule(GlobalNames::Forward_short_address, 8, RulesType::Switch, (int)GlobalNames::Forward_address_type, 0),
            Rule(GlobalNames::Forward_address_SSI, 24, RulesType::Switch, (int)GlobalNames::Forward_address_type, 1),
            Rule(GlobalNames::Forward_address_SSI, 24, RulesType::Switch, (int)GlobalNames::Forward_address_type, 2),
            Rule(GlobalNames::Forward_address_extension, 24, RulesType::Switch, (int)GlobalNames::Forward_address_type, 2),
            Rule(GlobalNames::Reserved, 0, RulesType::JampNot, (int)GlobalNames::Forward_address_type, 3, 99),
            Rule(GlobalNames::Number_subscriber_number_digits, 8),
            Rule(GlobalNames::Reserved, 8, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 1),
            Rule(GlobalNames::Reserved, 8, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 2),
            Rule(GlobalNames::Reserved, 16, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 3),
            Rule(GlobalNames::Reserved, 16, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 4),
            Rule(GlobalNames::Reserved, 24, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 5),
            Rule(GlobalNames::Reserved, 24, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 6),
            Rule(GlobalNames::Reserved, 32, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 7),
            Rule(GlobalNames::Reserved, 32, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 8),
            Rule(GlobalNames::Reserved, 40, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 9),
            Rule(GlobalNames::Reserved, 40, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 10),
            Rule(GlobalNames::Reserved, 48, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 11),
            Rule(GlobalNames::Reserved, 48, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 12),
            Rule(GlobalNames::Reserved, 56, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 13),
            Rule(GlobalNames::Reserved, 56, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 14),
            Rule(GlobalNames::Reserved, 64, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 15),
            Rule(GlobalNames::Reserved, 64, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 16),
            Rule(GlobalNames::Reserved, 72, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 17),
            Rule(GlobalNames::Reserved, 72, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 18),
            Rule(GlobalNames::Reserved, 80, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 19),
            Rule(GlobalNames::Reserved, 80, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 20),
            Rule(GlobalNames::Reserved, 88, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 21),
            Rule(GlobalNames::Reserved, 88, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 22),
            Rule(GlobalNames::Reserved, 96, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 23),
            Rule(GlobalNames::Reserved, 96, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 24),
};

inline const std::vector<Rule> d_Sds_TL_ReportRules = {

            Rule(GlobalNames::Acknowledgement_required, 1),
            Rule(GlobalNames::Reserved, 2),
            Rule(GlobalNames::Storage_forward_control, 1),
            Rule(GlobalNames::Delivery_status, 8),
            Rule(GlobalNames::Message_reference, 8),
            Rule(GlobalNames::Presence_bit, 0, RulesType::Jamp, (int)GlobalNames::Storage_forward_control, 0, 99),
            Rule(GlobalNames::Validity_period, 5),
            Rule(GlobalNames::Forward_address_type, 3),
            Rule(GlobalNames::Forward_short_address, 8, RulesType::Switch, (int)GlobalNames::Forward_address_type, 0),
            Rule(GlobalNames::Forward_address_SSI, 24, RulesType::Switch, (int)GlobalNames::Forward_address_type, 1),
            Rule(GlobalNames::Forward_address_SSI, 24, RulesType::Switch, (int)GlobalNames::Forward_address_type, 2),
            Rule(GlobalNames::Forward_address_extension, 24, RulesType::Switch, (int)GlobalNames::Forward_address_type, 2),
            Rule(GlobalNames::Reserved, 0, RulesType::JampNot, (int)GlobalNames::Forward_address_type, 3, 99),
            Rule(GlobalNames::Number_subscriber_number_digits, 8),
            Rule(GlobalNames::Reserved, 8, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 1),
            Rule(GlobalNames::Reserved, 8, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 2),
            Rule(GlobalNames::Reserved, 16, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 3),
            Rule(GlobalNames::Reserved, 16, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 4),
            Rule(GlobalNames::Reserved, 24, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 5),
            Rule(GlobalNames::Reserved, 24, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 6),
            Rule(GlobalNames::Reserved, 32, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 7),
            Rule(GlobalNames::Reserved, 32, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 8),
            Rule(GlobalNames::Reserved, 40, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 9),
            Rule(GlobalNames::Reserved, 40, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 10),
            Rule(GlobalNames::Reserved, 48, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 11),
            Rule(GlobalNames::Reserved, 48, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 12),
            Rule(GlobalNames::Reserved, 56, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 13),
            Rule(GlobalNames::Reserved, 56, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 14),
            Rule(GlobalNames::Reserved, 64, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 15),
            Rule(GlobalNames::Reserved, 64, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 16),
            Rule(GlobalNames::Reserved, 72, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 17),
            Rule(GlobalNames::Reserved, 72, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 18),
            Rule(GlobalNames::Reserved, 80, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 19),
            Rule(GlobalNames::Reserved, 80, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 20),
            Rule(GlobalNames::Reserved, 88, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 21),
            Rule(GlobalNames::Reserved, 88, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 22),
            Rule(GlobalNames::Reserved, 96, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 23),
            Rule(GlobalNames::Reserved, 96, RulesType::Switch, (int)GlobalNames::Number_subscriber_number_digits, 24),
};

inline const std::vector<Rule> sds_LocationShortRules = {

            Rule(GlobalNames::Time_elapsed, 2),
            Rule(GlobalNames::Longitude, 25),
            Rule(GlobalNames::Latitude, 24),
            Rule(GlobalNames::Position_error, 3),
            Rule(GlobalNames::Horizontal_velocity, 7),
            Rule(GlobalNames::Direction_of_travel, 4),
            Rule(GlobalNames::Options_bit, 1 , RulesType::Options_bit),
            Rule(GlobalNames::Reason_for_sending, 8),
            Rule(GlobalNames::User_defined_data, 8)
};

inline const std::vector<Rule> sds_ImmediateLocRepRules = {

            Rule(GlobalNames::Request_response, 1),
            Rule(GlobalNames::Report_type, 2),
            Rule(GlobalNames::T5_el_ident, 5),
            Rule(GlobalNames::T5_el_length, 6),
            Rule(GlobalNames::T5_el_length_ex, 7, RulesType::Switch, (int)GlobalNames::T5_el_length, 0),
};

inline const std::vector<Rule> sds_SimpleTextRules = {

            Rule(GlobalNames::Time_stamp_used, 1),
            Rule(GlobalNames::Text_coding_scheme, 7),
            Rule(GlobalNames::Timeframe_type, 2 ,RulesType::Switch, (int)GlobalNames::Time_stamp_used, 1),
            Rule(GlobalNames::Reserved, 2, RulesType::Switch, (int)GlobalNames::Time_stamp_used, 1),
            Rule(GlobalNames::Month, 4 ,RulesType::Switch, (int)GlobalNames::Time_stamp_used, 1),
            Rule(GlobalNames::Day, 5 ,RulesType::Switch, (int)GlobalNames::Time_stamp_used, 1),
            Rule(GlobalNames::Hour, 5 ,RulesType::Switch, (int)GlobalNames::Time_stamp_used, 1),
            Rule(GlobalNames::Minute, 6 ,RulesType::Switch, (int)GlobalNames::Time_stamp_used, 1),
};

}
