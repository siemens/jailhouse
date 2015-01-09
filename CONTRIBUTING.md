Contributing to Jailhouse
=========================

Contributions to Jailhouse are always welcome. This document explains the
general requirements on contributions and the recommended preparation steps. It
also sketches the typical integration process of patches.


Contribution Checklist
----------------------

- use git to manage your changes [*recomended*]

- follow Documentation/coding-style.txt coding style [**required**]

- structure patches logically, in small steps [**required**]
    - one separable functionality/fix/refactoring = one patch
    - do not mix those there in a single patch
    - after each patch, the tree still has to build and work, i.e. do not add
      even temporary breakages inside a patch series (helps when tracking down
      bugs)
    - use `git rebase -i` to restructure a patch series

- base patches on top of latest master or - if there are dependencies - on next
  (note: next is an integration branch that may change non-linearly)

- test patches sufficiently (obvious, but...) [**required**]
    - no regressions are caused in affected code
    - seemingly unaffected architectures still build (use Travis CI e.g.)
    - the world is still spinning

- add signed-off to all patches [**required**]
    - to certify the "Developer's Certificate of Origin" according to "Sign
      your work" in https://www.kernel.org/doc/Documentation/SubmittingPatches
    - check with your employer when not working on your own!

- post patches to mailing list [**required**]
    - use `git format-patch/send-email` if possible
    - send patches inline, do not append them
    - no HTML emails!
    - CC people who you think should look at the patches, e.g.
      - affected maintainers (see areas of responsibility below)
      - someone who wrote a change that is fixed or reverted by you now
      - who commented on related changes in the recent past
      - who otherwise has expertise and is interested in the topic
    - pull requests on github are only optional

- post follow-up version(s) if feedback requires this

- send reminder if nothing happened after about a week


Contribution Integration Process
--------------------------------

1. patch reviews performed on mailing list
    * at least by maintainers, but everyone is invited
    * feedback has to consider design, functionality and style
    * simpler and clearer code preferred, even if original code works fine

2. accepted patches merged into next branch

3. further testing done by community, including CI build tests

4. if no new problems or discussions showed up, acceptance into master
    * grace period for master: about 3 days
    * urgent fixes may be applied sooner

github facilities are not used for the review process so that people can follow
all changes and related discussions at a single stop, the mailing list. This
may change in the future if github should improve their email integration.


Areas of responsibility
-----------------------

Jailhouse is rather small. Nevertheless, there are different people involved in
different areas of its code. The following list shall give an overview on who
is working in which area and should be involved when discussing changes:

Jan Kiszka <jan.kiszka@siemens.com>:
 - overall Jailhouse maintenance
 - committer to official repository

Valentine Sinitsyn <valentine.sinitsyn@gmail.com>:
 - AMD64 support

Henning Schild <henning.schild@siemens.com>:
 - inter-cell communication
 - configuration file generator
