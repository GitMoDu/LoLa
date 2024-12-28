// LinkDisplayDrawer.h

#ifndef _LINK_DISPLAY_DRAWER_h
#define _LINK_DISPLAY_DRAWER_h

#include <ILoLaInclude.h>
#include "Elements.h"
#include "Pallete.h"

namespace LoLa::Display
{
	namespace LinkIndicator
	{
		static constexpr uint8_t Width = Elements::LinkIndicator::Width;
		static constexpr uint8_t Height = Elements::LinkIndicator::Height;

		template<typename Layout>
		class DrawerWrapper : public virtual Elements::LinkUpdate::IListener, public MultiDrawerWrapper<4>
		{
		public:
			using Base = MultiDrawerWrapper<4>;
			using Base::AddDrawer;

		private:
			Elements::LinkIndicator::LinkLiveDrawer<Layout> LinkLiveDrawer{};
			Elements::LinkIndicator::LinkOffDrawer<Layout> LinkOffDrawer{};
			Elements::LinkIndicator::LinkSearchingDrawer<Layout> LinkSearchingDrawer{};
			Elements::LinkUpdate::Drawer LinkUpdater;

		private:
			bool LinkOn = true;
			bool Enabled = false;

		public:
			DrawerWrapper(ILoLaLink& link)
				: Elements::LinkUpdate::IListener()
				, Base()
				, LinkUpdater(link, *this)
			{
				LinkLiveDrawer.Neutral.SetFrom(Assets::Pallete::Dark);
				LinkLiveDrawer.RxForeground.SetFrom(Assets::Pallete::Rx);
				LinkLiveDrawer.TxForeground.SetFrom(Assets::Pallete::Tx);
				LinkLiveDrawer.AgeOk = Assets::Pallete::Ok;
				LinkLiveDrawer.AgeNotOk = Assets::Pallete::NotOk;

				LinkSearchingDrawer.Background.SetFrom(Assets::Pallete::Dark);
				LinkSearchingDrawer.Foreground.SetFrom(Assets::Pallete::White);
				LinkSearchingDrawer.NotOk.SetFrom(Assets::Pallete::NotOk);
			}

			virtual void SetEnabled(const bool enabled)
			{
				Enabled = enabled;
			}

			void Set(const bool linkOn)
			{
				LinkOn = linkOn;

				LinkSearchingDrawer.SetEnabled(false);
				LinkOffDrawer.SetEnabled(false);
				LinkLiveDrawer.SetEnabled(false);

				if (LinkOn)
				{
					LinkSearchingDrawer.SetEnabled(true);
				}
				else
				{
					LinkOffDrawer.SetEnabled(true);
				}
			}

			virtual void OnLinkedUpdate(LoLaLinkExtendedStatus& linkStatus) final
			{
				if (Enabled)
				{
					LinkOffDrawer.SetEnabled(false);
					LinkSearchingDrawer.SetEnabled(false);
					LinkLiveDrawer.Set(linkStatus.Quality.RxRssi, linkStatus.Quality.Age, linkStatus.RxCount, linkStatus.TxCount);
				}
				else
				{
					LinkLiveDrawer.SetEnabled(false);
					LinkSearchingDrawer.SetEnabled(false);
					LinkOffDrawer.SetEnabled(false);
				}
			}

			virtual void OnUnlinkedUpdate() final
			{
				if (Enabled)
				{
					LinkLiveDrawer.SetEnabled(false);

					if (LinkOn)
					{
						LinkSearchingDrawer.SetEnabled(true);
					}
					else
					{
						LinkOffDrawer.SetEnabled(true);
					}
				}
				else
				{
					LinkLiveDrawer.SetEnabled(false);
					LinkSearchingDrawer.SetEnabled(false);
					LinkOffDrawer.SetEnabled(false);
				}
			}

			const bool AddDrawers()
			{
				ClearDrawers();

				OnUnlinkedUpdate();

				return AddDrawer(LinkUpdater)
					&& AddDrawer(LinkLiveDrawer)
					&& AddDrawer(LinkOffDrawer)
					&& AddDrawer(LinkSearchingDrawer);
			}
		};
	}

	namespace LinkDebug
	{
		template<typename Layout>
		static constexpr uint8_t ClockHeight()
		{
			return Elements::LinkClock::ClockLayoutCalculator<Layout::Width(), Layout::Height() - 2 - Elements::LinkIndicator::Height >::FontHeight();
		}

		// Font size aware display height: i.e. actual height.
		template<typename Layout>
		static constexpr uint8_t DisplayHeight()
		{
			return Elements::LinkIndicator::Height + 2 + ClockHeight<Layout>();
		}

		template<typename Layout>
		class DrawerWrapper : public virtual Elements::LinkUpdate::IListener, public MultiDrawerWrapper<8>
		{
		public:
			using Base = MultiDrawerWrapper<8>;
			using Base::AddDrawer;

		private:
			struct DrawerLayout
			{
				static constexpr uint8_t ProgressWidth = 4;

				using LinkIndicator = LayoutElement<Layout::X(), Layout::Y(), Elements::LinkIndicator::Width, Elements::LinkIndicator::Height>;
				using Clock = LayoutElement<Layout::X(), Layout::Y() + LinkIndicator::Height() + 2, Layout::Width(), ClockHeight<Layout>()>;

				using QualityRx = LayoutElement<Layout::X() + LinkIndicator::Width() + 3, Layout::Y(), ProgressWidth, LinkIndicator::Height()>;
				using QualityTx = LayoutElement<QualityRx::X() + ProgressWidth + 1, Layout::Y(), ProgressWidth, LinkIndicator::Height()>;
				using QualityClock = LayoutElement<QualityTx::X() + ProgressWidth + 1, Layout::Y(), ProgressWidth, LinkIndicator::Height()>;
			};

		private:
			Elements::LinkIndicator::LinkLiveDrawer<typename DrawerLayout::LinkIndicator> LinkLiveDrawer{};
			Elements::LinkIndicator::LinkOffDrawer<typename DrawerLayout::LinkIndicator> LinkOffDrawer{};
			Elements::LinkIndicator::LinkSearchingDrawer<typename DrawerLayout::LinkIndicator> LinkSearchingDrawer{};
			Elements::LinkClock::Drawer<typename DrawerLayout::Clock> ClockDrawer{};
			Elements::VerticalProgress::Drawer<typename DrawerLayout::QualityRx> QualityRxDrawer{};
			Elements::VerticalProgress::Drawer<typename DrawerLayout::QualityTx> QualityTxDrawer{};
			Elements::VerticalProgress::Drawer<typename DrawerLayout::QualityClock> QualityClockDrawer{};

			Elements::LinkUpdate::Drawer LinkUpdater;

		public:
			DrawerWrapper(ILoLaLink& link)
				: Elements::LinkUpdate::IListener()
				, Base()
				, LinkUpdater(link, *this)
			{
				LinkLiveDrawer.Neutral.SetFrom(Assets::Pallete::Dark);
				LinkLiveDrawer.RxForeground.SetFrom(Assets::Pallete::Rx);
				LinkLiveDrawer.TxForeground.SetFrom(Assets::Pallete::Tx);
				LinkLiveDrawer.AgeOk = Assets::Pallete::Ok;
				LinkLiveDrawer.AgeNotOk = Assets::Pallete::NotOk;

				LinkSearchingDrawer.Background.SetFrom(Assets::Pallete::Dark);
				LinkSearchingDrawer.Foreground.SetFrom(Assets::Pallete::White);
				LinkSearchingDrawer.NotOk.SetFrom(Assets::Pallete::NotOk);

				ClockDrawer.SetColor(Assets::Pallete::Darkish);

				QualityRxDrawer.Foreground.SetFrom(Assets::Pallete::Rx);
				QualityRxDrawer.Outline.SetFrom(Assets::Pallete::Dark);

				QualityTxDrawer.Foreground.SetFrom(Assets::Pallete::Tx);
				QualityTxDrawer.Outline.SetFrom(Assets::Pallete::Dark);

				QualityClockDrawer.Foreground.SetFrom(Assets::Pallete::Darkish);
				QualityClockDrawer.Outline.SetFrom(Assets::Pallete::Dark);
			}

			virtual void OnLinkedUpdate(LoLaLinkExtendedStatus& linkStatus) final
			{
				LinkOffDrawer.SetEnabled(false);
				LinkSearchingDrawer.SetEnabled(false);

				LinkLiveDrawer.Set(linkStatus.Quality.RxRssi, linkStatus.Quality.Age, linkStatus.RxCount, linkStatus.TxCount);
				ClockDrawer.Set(linkStatus.DurationSeconds);

				QualityRxDrawer.Set(linkStatus.Quality.RxDrop);
				QualityTxDrawer.Set(linkStatus.Quality.TxDrop);
				QualityClockDrawer.Set(linkStatus.Quality.ClockSync);
			}

			virtual void OnUnlinkedUpdate() final
			{
				LinkLiveDrawer.SetEnabled(false);
				LinkOffDrawer.SetEnabled(false);
				LinkSearchingDrawer.SetEnabled(true);

				ClockDrawer.SetEnabled(false);

				QualityRxDrawer.SetEnabled(false);
				QualityTxDrawer.SetEnabled(false);
				QualityClockDrawer.SetEnabled(false);
			}

			const bool AddDrawers()
			{
				ClearDrawers();

				OnUnlinkedUpdate();

				return AddDrawer(LinkUpdater)
					&& AddDrawer(LinkLiveDrawer)
					&& AddDrawer(LinkOffDrawer)
					&& AddDrawer(LinkSearchingDrawer)
					&& AddDrawer(ClockDrawer)
					&& AddDrawer(QualityRxDrawer)
					&& AddDrawer(QualityTxDrawer)
					&& AddDrawer(QualityClockDrawer);
			}
		};
	}
}
#endif