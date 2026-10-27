#pragma once

#include <CamerasPlugin/Core/CameraModifier.h>
#include <Core/World/Component.h>
#include <Core/World/World.h>

class plCameraViewComponent;
struct plMsgCameraImpulse;
struct plMsgCameraRequestView;
struct plMsgCameraReleaseView;

using plCameraDirectorComponentManager = plComponentManager<class plCameraDirectorComponent, plBlockStorageType::Compact>;

/// \brief Decides which camera view a player sees, blends between views and applies the camera modifier stack.
///
/// Put it next to the Main View plCameraComponent. Views ask to be shown through requests with a priority: the
/// player's own view (priority 10), camera volumes, scripts, sequences. The highest priority wins, the newest wins a
/// tie, and every change blends from wherever the camera is at that moment, so an interrupted blend never pops.
/// After the view, the modifiers run in two stages, framing then effects: the active view's own modifiers first,
/// then the director's.
class PL_CAMERAS_DLL plCameraDirectorComponent : public plComponent
{
  PL_DECLARE_COMPONENT_TYPE(plCameraDirectorComponent, plComponent, plCameraDirectorComponentManager);

  //////////////////////////////////////////////////////////////////////////
  // plComponent

public:
  virtual void SerializeComponent(plWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(plWorldReader& inout_stream) override;

protected:
  virtual void OnActivated() override;
  virtual void OnDeactivated() override;
  virtual void OnSimulationStarted() override;

  //////////////////////////////////////////////////////////////////////////
  // plCameraDirectorComponent

public:
  plCameraDirectorComponent();
  ~plCameraDirectorComponent();

  /// Which local player this director serves. Only matters with several directors (split-screen hooks).
  plUInt8 m_uiPlayer = 0; // [ property ]

  /// The player's own view. Empty uses the first view on this object or its children.
  void SetDefaultViewReference(const char* szReference); // [ property ]

  /// Blend used when neither the request nor the view sets one.
  plTime m_BlendTime = plTime::MakeFromSeconds(0.6); // [ property ]
  plEnum<plCameraBlendCurve> m_BlendCurve;          // [ property ]

  /// Whether to write focus distances from the Focus modifier and views to the camera.
  bool m_bWriteFocus = true; // [ property ]

  /// Applied to every view, after the view's own modifiers.
  plDynamicArray<plCameraModifier*> m_Modifiers; // [ property ]

  /// \brief Asks to show a view. Returns an id for ReleaseRequest. A zero duration lasts until released.
  ///
  /// A negative \a blendTime uses the view's or the director's blend.
  plUInt32 RequestView(plCameraViewComponent* pView, plInt32 iPriority, plTime duration, plTime blendTime, plCameraBlendCurve::Enum curve,
    plCameraRequestSource::Enum source);

  void ReleaseRequest(plUInt32 uiRequestId, plTime blendTime = plTime::MakeFromSeconds(-1.0));
  void ReleaseView(plComponentHandle hView, plTime blendTime = plTime::MakeFromSeconds(-1.0));

  /// \brief Replaces the player's own view.
  void SetPlayerView(plCameraViewComponent* pView);

  plCameraViewComponent* GetActiveView() const;
  plCameraViewComponent* GetPlayerView() const;
  const plCameraPose& GetOutputPose() const { return m_Output; }

  /// \brief Whether the camera is blending between two views right now.
  bool IsBlending() const { return m_fBlendT < 1.0f; } // [ scriptable ]

  void PlayImpulse(const plMsgCameraImpulse& msg);

  /// \brief Runs one frame. Called by plCameraWorldModule.
  void UpdateDirector(float fDeltaSeconds);

  // ---- script API ----

  /// \brief Shows the view on the object at \a szViewObject (a path or name). Zero duration lasts until released.
  plUInt32 ActivateView(const char* szViewObject, plInt32 iPriority, float fDurationSeconds, float fBlendSeconds); // [ scriptable ]

  /// \brief Releases every request for the view on \a szViewObject.
  void ReleaseViewByName(const char* szViewObject, float fBlendSeconds); // [ scriptable ]
  void ReleaseRequestById(plUInt32 uiRequestId);                        // [ scriptable ]
  void SetPlayerViewByName(const char* szViewObject);                    // [ scriptable ]
  void AddTrauma(float fTrauma);                                         // [ scriptable ]
  void AddRecoil(float fPitchDegrees, float fYawDegrees);                // [ scriptable ]
  void SetModifierEnabled(const char* szName, bool bEnabled);            // [ scriptable ]
  void SetModifierWeight(const char* szName, float fWeight);             // [ scriptable ]

  /// \brief Frames the point of interest on \a szObject now, through the Look At modifier, without its key.
  void LookAtObject(const char* szObject); // [ scriptable ]

  /// \brief Holds every Zoom modifier zoomed in (or releases them).
  void SetZoomed(bool bZoomed); // [ scriptable ]

  /// \brief Leans every Lean modifier: -1 fully left, 1 fully right, 0 upright.
  void SetLean(float fLean); // [ scriptable ]

  /// \brief The point of interest a Look At key press would frame right now, for the game's prompt.
  plComponentHandle GetBestPointOfInterest() const;

  /// \brief Calls \a func for every modifier of type T in the active view's and the director's stacks.
  template <typename T, typename Func>
  void ForEachModifier(Func func) const;

  /// \brief The active view's modifiers followed by the director's.
  void GetAllModifiers(plDynamicArray<plCameraModifier*>& out_modifiers) const;

  /// \brief Multiplies views' look input; zoom modifiers lower it. Read by views each frame.
  float GetLookScale() const { return m_fLookScale; }

private:
  const char* DummyGetter() const { return nullptr; }

  void OnMsgRequestView(plMsgCameraRequestView& msg);
  void OnMsgReleaseView(plMsgCameraReleaseView& msg);
  void OnMsgImpulse(plMsgCameraImpulse& msg);

  struct Request
  {
    plUInt32 m_uiId = 0;
    plComponentHandle m_hView;
    plInt32 m_iPriority = 0;
    double m_fEndTime = 0.0; ///< zero: open
    plTime m_BlendTime = plTime::MakeFromSeconds(-1.0);
    plEnum<plCameraBlendCurve> m_BlendCurve;
    plEnum<plCameraRequestSource> m_Source;
    plComponentHandle m_hVolume;
  };

  plCameraViewComponent* ResolvePlayerView();
  void UpdateVolumes();
  void PruneRequests();
  const Request* SelectTopRequest() const;
  void StartBlend(const Request& request, plCameraViewComponent* pView);
  plTime GetBlendTime(const Request& request, const plCameraViewComponent* pView) const;
  plCameraBlendCurve::Enum GetBlendCurve(const Request& request, const plCameraViewComponent* pView) const;
  void DrawDebug(const plCameraModifierContext& context) const;
  void PublishStats() const;

  /// \brief Steps aside while another camera holds the Main View (a sequence cut), and blends back afterwards.
  ///
  /// Returns true while yielding: the director then writes nothing.
  bool UpdateYield(class plCameraComponent* pOwnCamera);

  /// The camera's own field of view, read when play starts: what views with Field Of View zero show.
  float m_fCameraFov = 0.0f;

  bool m_bScannedMainViews = false;
  plHybridArray<plComponentHandle, 2> m_IgnoredMainViews; ///< Main View cameras that existed when simulation started
  plComponentHandle m_hYieldCamera;
  bool m_bYielding = false;

  plGameObjectHandle m_hDefaultViewObject;
  plComponentHandle m_hPlayerView;

  plDynamicArray<Request> m_Requests;
  plUInt32 m_uiNextRequestId = 1;
  plUInt32 m_uiActiveRequestId = plInvalidIndex;
  plComponentHandle m_hActiveView;

  plCameraPose m_BlendFrom;
  float m_fBlendT = 1.0f;
  plTime m_BlendDuration;
  plEnum<plCameraBlendCurve> m_ActiveCurve;

  /// Set when a request is released with its own blend back; used for the next change.
  plTime m_PendingBlendTime = plTime::MakeFromSeconds(-1.0);

  plCameraPose m_Output;
  bool m_bHasOutput = false;
  float m_fLookScale = 1.0f;
  double m_fTime = 0.0;
};

template <typename T, typename Func>
void plCameraDirectorComponent::ForEachModifier(Func func) const
{
  plHybridArray<plCameraModifier*, 16> modifiers;
  GetAllModifiers(modifiers);

  for (plCameraModifier* pModifier : modifiers)
  {
    if (pModifier->IsInstanceOf<T>())
      func(static_cast<T*>(pModifier));
  }
}
