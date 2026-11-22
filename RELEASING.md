# Releasing Ox

Every release publishes the source gem plus one precompiled native gem per
platform in `NATIVE_PLATFORMS` (see the Rakefile). The `Precompiled gems`
workflow (`.github/workflows/precompiled.yml`) builds and tests them all, and on
a `v*` tag in `ohler55/ox` its `publish` job pushes them to RubyGems.org.

## One-time setup

Do these once. Nothing is pushed until they are done; until then the `publish`
job fails at the credentials step.

### 1. Create the `release` environment on GitHub

In the repository: **Settings → Environments → New environment**, name it
`release`. Optionally, under **Deployment protection rules**:

- **Required reviewers**: add yourself, so every publish waits for an approval
  click on the workflow run. Recommended.
- **Deployment branches and tags**: choose *Selected branches and tags* and add
  a tag rule `v*`, so only release tags can use the environment.

### 2. Add a trusted publisher on RubyGems.org (recommended)

Trusted publishing lets the workflow get a short-lived push token from
RubyGems.org through GitHub's OIDC, so no API key is stored anywhere. It also
satisfies Ox's `rubygems_mfa_required`: RubyGems.org treats these tokens as
MFA-verified, so no one-time password is needed.

1. Sign in to RubyGems.org as an owner of `ox` and open
   <https://rubygems.org/gems/ox>.
2. In the sidebar choose **Trusted publishers**, then **Create**.
3. Fill in:

   | Field | Value |
   |---|---|
   | Repository owner | `ohler55` |
   | Repository name | `ox` |
   | Workflow filename | `precompiled.yml` |
   | Environment | `release` |

   Leave the *Workflow Repository* fields blank.
4. Click **Create Rubygem trusted publisher**.

The workflow filename and environment must match exactly, or the token exchange
is refused. If the workflow file is ever renamed, update the publisher.

### 2b. Or: use an API key instead

Only if trusted publishing is not an option. Be aware that if your RubyGems.org
account's MFA level is **UI and API**, every push with a personal API key needs
a one-time password, which the workflow cannot supply. A key only works from CI
if your MFA level is **UI and gem signin** and the key itself does not have MFA
enabled.

1. On RubyGems.org go to **Settings → API keys → New API key**. Give it only
   the **Push rubygem** scope, limit it to the `ox` gem, and set an expiry.
2. In the GitHub repository go to **Settings → Environments → release →
   Environment secrets → Add secret**. Name it `RUBYGEMS_API_TOKEN` and paste
   the key. Put it on the `release` environment rather than in repository
   secrets, so only the publish job can read it.

When `RUBYGEMS_API_TOKEN` is set the workflow uses it; when it is not, it uses
trusted publishing. Delete the secret to switch back.

## Cutting a release

1. Bump `Ox::VERSION` in `lib/ox/version.rb` and add the `CHANGELOG.md` entry.
   Commit and push to `develop` (and `master` if that is part of your flow), and
   let CI go green.
2. Tag and push the tag:

   ```sh
   git tag v2.14.30
   git push origin v2.14.30
   ```

   The tag must be `v` followed by `Ox::VERSION`; the workflow refuses to
   publish otherwise.
3. The `Precompiled gems` workflow builds and tests every gem. The
   `Collect gems for release` job prints the SHA-256 of each gem and what would
   be pushed. If you set required reviewers, approve the `publish` job.
4. `publish` pushes the native gems, then the source gem.

`bundle exec rake release` still works as before: it tags, pushes the tag and
pushes the source gem. The workflow then pushes the native gems and skips the
source gem, because it already exists on RubyGems.org. Either way, re-running
the `publish` job is safe; it skips anything already published.

## If something goes wrong

- **A build or test job fails:** nothing is published, because `publish` needs
  every job to pass. Fix the problem in a new version; RubyGems.org never
  accepts the same version and platform twice, even after a yank.
- **`publish` fails partway:** re-run the job. Gems already pushed are skipped.
- **The token exchange is refused:** check that the trusted publisher's
  workflow filename and environment match `precompiled.yml` and `release`.
- **A platform gem is broken after release:** `gem yank ox -v X.Y.Z --platform
  PLATFORM` removes just that platform. Users on it fall back to the source gem
  and compile it.

## Building locally

With Docker installed, `bundle exec rake gem:native` builds every native gem
into `pkg/`, and `bundle exec rake gem:native:x86_64-linux-gnu` builds one. The
local gems can be pushed by hand with `gem push`, which prompts for your OTP.
