# Testing Pull Requests (PRs)

When a PR is submitted, it is compiled automatically by AppVeyor CI. The AppVeyor build creates an artifact which contains the Flare executable and supporting DLLs needed to run.  You can easily download this artifact and run it to test out the PR before it is merged.

## Pre-Testing Notes
- You need to have Flare already installed.  Do not uninstall.
- It is recommended that you backup your `C:\Flare stuff` folder before testing a PR.
- The AppVeyor build does not contain an updated `C:\Flare stuff` folder so any PR changes to default configurations (i.e. stylesheets, FXs, brushes, etc) are not automatically included.
- It is recommended when testing to use new scenes that you can throw away.
  - If you decide to test on an existing scene, back it up first!

## Downloading and Testing AppVeyor Builds

- Go to the PR you are interested in checking out.

- Scroll to the bottom of the page and look for the green-outlined box with `All checks have passed` in it. Click on it to expend.

![](./testing_pr_1.png)

- Look for where it says `AppVeyor build succeeded` and click on the `...` next to it, select `View Details`.

![](./testing_pr_2.png)

- Click on `Configuration: Release`.

![](./testing_pr_3.JPG)

- On the right side of the page, click on `Artifacts`.

![](./testing_pr_4.JPG)

- Click on the Flare `.zip` file to begin the download.

![](./testing_pr_5.JPG)

## Downloading and Testing Github Action Builds

- Similar to the previous steps, but start clicking different lines from step 3.

![](./testing_pr_6.png)

- On the **Details** page, click **Summary** on the left side.

![](./testing_pr_7.png)

- Scroll down to find the **Download** button.

  ![](./testing_pr_8.png)



---



- Once download is complete, go to your Downloads folder and extract the contents of the .zip file anywhere on your computer.
 - You should extract into a separate folder. 
 - ⚠️ **Do not overwrite your current Flare installation!**
- Enter the folder you extracted everything into and double-click the Flare executable to start.
- Test away!  Report any suggestions or issues related to the change in the PR comments.
- You can safely delete the .zip file and the folder you created when you are done.

