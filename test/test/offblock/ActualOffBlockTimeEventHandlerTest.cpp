#include "pch/pch.h"
#include "offblock/ActualOffBlockTimeEventHandler.h"
#include "flightplan/StoredFlightplanCollection.h"
#include "mock/MockEuroScopeCFlightplanInterface.h"
#include "mock/MockEuroScopeCRadarTargetInterface.h"
#include "flightplan/StoredFlightplan.h"
#include "datablock/DisplayTime.h"
#include "ownership/AirfieldOwnershipManager.h"
#include "controller/ActiveCallsignCollection.h"
#include "controller/ActiveCallsign.h"
#include "controller/ControllerPosition.h"
#include "airfield/AirfieldCollection.h"
#include "airfield/AirfieldModel.h"

using UKControllerPlugin::Datablock::ActualOffBlockTimeEventHandler;
using UKControllerPlugin::Flightplan::StoredFlightplanCollection;
using UKControllerPluginTest::Euroscope::MockEuroScopeCFlightPlanInterface;
using UKControllerPluginTest::Euroscope::MockEuroScopeCRadarTargetInterface;
using UKControllerPlugin::Flightplan::StoredFlightplan;
using UKControllerPlugin::Datablock::DisplayTime;
using UKControllerPlugin::Ownership::AirfieldOwnershipManager;
using UKControllerPlugin::Controller::ActiveCallsignCollection;
using UKControllerPlugin::Controller::ActiveCallsign;
using UKControllerPlugin::Controller::ControllerPosition;
using UKControllerPlugin::Airfield::AirfieldCollection;
using UKControllerPlugin::Controller::ControllerPosition;
using UKControllerPlugin::Datablock::DisplayTime;

using ::testing::Test;
using ::testing::NiceMock;
using ::testing::Return;

namespace UKControllerPluginTest {
    namespace Datablock {

        class ActualOffBlockTimeEventHandlerTest : public Test
        {
            public:
                void SetUp()
                {
                    this->flightplans.UpdatePlan(StoredFlightplan("BAW123", "EGKK", "EGPH"));
                    this->handler = std::make_unique<ActualOffBlockTimeEventHandler>(
                        this->flightplans,
                        this->timeFormat
                    );
                };

                DisplayTime timeFormat;
                StoredFlightplanCollection flightplans;
                NiceMock<MockEuroScopeCFlightPlanInterface> mockFlightplan;
                NiceMock<MockEuroScopeCRadarTargetInterface> mockRadarTarget;
                std::unique_ptr<ActualOffBlockTimeEventHandler> handler;
        };

        TEST_F(ActualOffBlockTimeEventHandlerTest, ControllerFlightplanDataEventSetsAobtIfPush)
        {
            ON_CALL(mockFlightplan, GetCallsign())
                .WillByDefault(Return("BAW123"));

            ON_CALL(mockFlightplan, GetOrigin())
                .WillByDefault(Return("EGGD"));

            ON_CALL(mockFlightplan, GetGroundState())
                .WillByDefault(Return("PUSH"));

            this->handler->ControllerFlightPlanDataEvent(
                this->mockFlightplan,
                EuroScopePlugIn::CTR_DATA_TYPE_GROUND_STATE
            );

            int64_t seconds = std::chrono::duration_cast<std::chrono::seconds> (
                std::chrono::system_clock::now() -
                    this->flightplans.GetFlightplanForCallsign("BAW123").GetActualOffBlockTime()
            )
            .count();

            EXPECT_GE(seconds, 0);
            EXPECT_LT(seconds, 2);
        }

        TEST_F(ActualOffBlockTimeEventHandlerTest, ControllerFlightplanDataEventResetsAobtIfPushReselected)
        {
            this->flightplans.GetFlightplanForCallsign("BAW123").SetActualOffBlockTime(
                std::chrono::system_clock::now() - std::chrono::minutes(3)
            );

            ON_CALL(mockFlightplan, GetCallsign())
                .WillByDefault(Return("BAW123"));

            ON_CALL(mockFlightplan, GetOrigin())
                .WillByDefault(Return("EGGD"));

            ON_CALL(mockFlightplan, GetGroundState())
                .WillByDefault(Return("PUSH"));

            this->handler->ControllerFlightPlanDataEvent(
                this->mockFlightplan,
                EuroScopePlugIn::CTR_DATA_TYPE_GROUND_STATE
            );

            int64_t seconds = std::chrono::duration_cast<std::chrono::seconds> (
                std::chrono::system_clock::now() -
                this->flightplans.GetFlightplanForCallsign("BAW123").GetActualOffBlockTime()
            )
                .count();

            EXPECT_LE(seconds, 2);
        }

        TEST_F(ActualOffBlockTimeEventHandlerTest, ControllerFlightplanDataEventDoesntSetAobtOnTaxi)
        {
            ON_CALL(mockFlightplan, GetCallsign())
                .WillByDefault(Return("BAW123"));

            ON_CALL(mockFlightplan, GetOrigin())
                .WillByDefault(Return("EGGD"));

            ON_CALL(mockFlightplan, GetGroundState())
                .WillByDefault(Return("TAXI"));

            this->handler->ControllerFlightPlanDataEvent(
                this->mockFlightplan,
                EuroScopePlugIn::CTR_DATA_TYPE_GROUND_STATE
            );

            EXPECT_EQ(
                this->flightplans.GetFlightplanForCallsign("BAW123").GetActualOffBlockTime(),
                (std::chrono::system_clock::time_point::max)()
            );
        }

        TEST_F(ActualOffBlockTimeEventHandlerTest, ControllerFlightplanDataEventDoesntSetAobtOnDeparture)
        {
            ON_CALL(mockFlightplan, GetCallsign())
                .WillByDefault(Return("BAW123"));

            ON_CALL(mockFlightplan, GetOrigin())
                .WillByDefault(Return("EGGD"));

            ON_CALL(mockFlightplan, GetGroundState())
                .WillByDefault(Return("DEPA"));

            this->handler->ControllerFlightPlanDataEvent(
                this->mockFlightplan,
                EuroScopePlugIn::CTR_DATA_TYPE_GROUND_STATE
            );

            EXPECT_EQ(
                this->flightplans.GetFlightplanForCallsign("BAW123").GetActualOffBlockTime(),
                (std::chrono::system_clock::time_point::max)()
            );
        }

        TEST_F(ActualOffBlockTimeEventHandlerTest, ControllerFlightplanDataEventDoesntSetAobtIfNotGroundStateChange)
        {
            this->handler->ControllerFlightPlanDataEvent(
                this->mockFlightplan,
                EuroScopePlugIn::CTR_DATA_TYPE_COMMUNICATION_TYPE
            );

            EXPECT_EQ(
                this->flightplans.GetFlightplanForCallsign("BAW123").GetActualOffBlockTime(),
                (std::chrono::system_clock::time_point::max)()
            );
        }

        TEST_F(ActualOffBlockTimeEventHandlerTest, ControllerFlightplanDataEventDoesntSetAobtIfNoFlightplan)
        {
            this->flightplans.RemovePlanByCallsign("BAW123");

            ON_CALL(mockFlightplan, GetCallsign())
                .WillByDefault(Return("BAW123"));

            EXPECT_NO_THROW(
                this->handler->ControllerFlightPlanDataEvent(
                    this->mockFlightplan,
                    EuroScopePlugIn::CTR_DATA_TYPE_GROUND_STATE
                )
            );
        }

        TEST_F(ActualOffBlockTimeEventHandlerTest, TagItemDescriptionReturnsDescription)
        {
            EXPECT_TRUE(this->handler->GetTagItemDescription() == "Actual Off-block Time");
        }

        TEST_F(ActualOffBlockTimeEventHandlerTest, TagItemDataReturnsBlankIfNoData)
        {
            ON_CALL(mockFlightplan, GetCallsign())
                .WillByDefault(Return("BAW123"));

            EXPECT_EQ(
                this->timeFormat.GetUnknownTimeFormat(),
                this->handler->GetTagItemData(this->mockFlightplan, this->mockRadarTarget)
            );
        }

        TEST_F(ActualOffBlockTimeEventHandlerTest, TagItemDataReturnsBlankIfNoFlightplan)
        {
            ON_CALL(mockFlightplan, GetCallsign())
                .WillByDefault(Return("BAW124"));

            EXPECT_EQ(
                this->timeFormat.GetUnknownTimeFormat(),
                this->handler->GetTagItemData(this->mockFlightplan, this->mockRadarTarget)
            );
        }

        TEST_F(ActualOffBlockTimeEventHandlerTest, TagItemDataReturnsAobtIfSet)
        {
            ON_CALL(mockFlightplan, GetCallsign())
                .WillByDefault(Return("BAW123"));

            this->flightplans.GetFlightplanForCallsign("BAW123").SetActualOffBlockTime(
                std::chrono::system_clock::now()
            );

            EXPECT_TRUE(
                this->handler->GetTagItemData(this->mockFlightplan, this->mockRadarTarget) ==
                    this->timeFormat.FromSystemTime()
            );
        }
    }  // namespace Datablock
}  // namespace UKControllerPluginTest
