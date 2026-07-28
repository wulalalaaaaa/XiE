# UI2D Widget Lifecycle

## State sequence

The fixed lifecycle is:

```text
Constructing
  -> build owned UINodes
  -> apply initial common and type-specific properties
  -> register the Widget record
  -> install behavior connections
Mounted
  -> property updates and routed events
  -> enqueue destroy/reparent requests
Destroying
  -> destroy child Widgets
  -> disconnect all event and signal connections
  -> detach from the Widget parent
  -> destroy the owned Scene subtree
Destroyed
  -> release the handle slot
```

Construction validates the parent and required value ranges before registration. If node construction or record registration fails, the created Scene root is destroyed; because behavior listeners are installed only after a successful registration, a failed construction leaves neither owned nodes nor listeners.

## Safe destruction

`UIWidgetRuntime::Destroy` marks the record as already queued and appends a `DestroyWidgetMutation`. A second request is rejected safely. Registry destruction itself refuses to run during listener dispatch, which prevents accidental bypass of the safe point through the lower-level API.

At flush time, child Widgets are destroyed before their parent. Connections are removed before the Scene root is destroyed. The Scene invalidation callback informs focus, Widget ownership, listeners, interaction state, animator, style resolver, and input router. The record is then released and its generation can never validate again.

If a caller destroys an owned `UINode` directly, invalidation removes that node's ownership entry and queues destruction of the owning Widget. This keeps the two lifetimes synchronized without requiring all raw Scene nodes to become Widgets.

## Callback guarantees

Event connections revalidate `UIWidgetHandle` immediately before invoking user code. Activated and checked-changed callbacks are copied before invocation, allowing a callback to disconnect itself or enqueue Widget destruction safely. New connections cannot be registered for a record that is not Mounted. Recursive button activation is rejected by `activationInProgress`.

Callbacks may request Widget destruction or reparenting, update Widget properties, request focus, start an animation, or enqueue a Scene mutation. Deferred queues ensure those operations do not mutate an active listener traversal.

## Reparenting

A reparent request is validated again at flush time. Both handles must still be live, the new parent must belong to the same window registry, and the relation must not create a Widget cycle. A successful operation reparents the Widget root to the parent's child host and updates both Widget records in the same safe point. `UIScene` remains the final source of layout and traversal order.

## Window teardown

`UIWindowRuntime` explicitly clears its owned `UIWidgetRuntime` in its destructor while Scene and listener storage are still alive. Pending Widget mutations are discarded, all remaining Widget roots are destroyed, and all connection handles are invalidated before member destruction continues. This order prevents Widget lifetime from exceeding the services it uses.

Hidden windows deliberately postpone Widget mutation flushes. When the window becomes visible again, pending mutations run before layout and render construction. No Widget-owned timer or frame loop bypasses this policy.
